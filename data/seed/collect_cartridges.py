"""Builds published_cartridges.json: factory loads with the muzzle velocity
and BC their makers publish, from sources/cartridges_<maker>.jsonl (each row
copied from the maker's page by sources/cartridges_<maker>.py).

    python collect_cartridges.py

* A load without a muzzle velocity, or without a BC (on its page or, for
  Nosler, on its bullet's page in published_bullets.json), is left out.
* The same load sold under several product codes (box sizes, case finishes)
  becomes one cartridge listing every code.
* A G7 BC whose G7/G1 ratio no real bullet has is dropped, as in
  collect_bullets.py.
"""

import collections
import json
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "sources"))

from cartridges_common import DIAMETER, library_bullet  # noqa: E402

# Our calibre names -> the names the rifle catalog uses (rifle and cartridge
# are matched by calibre).
CALIBRE_NAMES = {".30-06": ".30-06 Sprg"}
# Bullet diameter -> the calibre label of the bullet library.
BULLET_CALIBRES = {.224: ".224", .243: "6mm", .264: "6.5mm", .284: "7mm", .308: ".308", .338: ".338",
                   .311: ".311", .221: ".221"}
# Product lines that say nothing about the load.
GENERIC_LINES = re.compile(r"^(rifle ammunition.*|barnaul|civil|rifle|shop (?:by|all) calib.*|\d.*)$", re.I)
RATIO = (0.40, 0.60)


def clean(text):
    text = re.sub(r"[®™]", "", (text or "").replace("‑", "-"))
    text = re.sub(r"^\d+(?:\.\d+)?\s*gr\s+", "", text.strip())
    text = re.sub(r"\s+", " ", text).strip()
    return text.title() if text == text.lower() else text


def words(text):
    return re.sub(r"[^a-z0-9]+", " ", text.lower()).split()


def describe(r):
    """The load's own words: its product line, then the bullet if it adds something."""
    line = "" if GENERIC_LINES.match(r["line"].strip()) else clean(r["line"])
    bullet = clean(r["bullet"])
    if not line or " ".join(words(line)) in " ".join(words(bullet)):
        return bullet or line
    if not bullet or " ".join(words(bullet)) in " ".join(words(line)):
        return line
    # "Gold Medal Sierra Matchking" + "Sierra Matchking Boat-Tail HP": say the overlap once.
    lw, bw = words(line), words(bullet)
    for n in range(min(len(lw), len(bw)), 0, -1):
        if lw[-n:] == bw[:n]:
            return f"{line} {' '.join(bullet.split()[n:])}".strip()
    return f"{line} {bullet}"


def bc_of(r, library):
    """(g1, g7, length_in, where) or None."""
    g1, g7, length, where = r["bc_g1"], r["bc_g7"], None, "page"
    # A BC printed without its "0." (Norma 20166512: "428").
    if g1 and g1 > 2:
        g1, where = g1 / 1000, f'page (printed "{g1:g}", read as {g1 / 1000:g})'
    if not (g1 or g7):
        b = library_bullet(r, library)
        if not b:
            return None
        g1, g7, length, where = b.get("g1"), b.get("g7"), b.get("length_in"), "bullet " + b["name"]
    if g1 and g7 and not RATIO[0] <= g7 / g1 <= RATIO[1]:
        g7 = None
    return g1, g7, length, where


def main():
    library = json.loads((HERE / "published_bullets.json").read_text(encoding="utf-8"))["bullets"]
    rows = []
    for path in sorted((HERE / "sources").glob("cartridges_*.jsonl")):
        rows += [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    left_out = collections.Counter()
    loads = {}
    for r in rows:
        bc = bc_of(r, library)
        if not r["velocity"]["value"] or not r["mass_gr"] or not bc:
            left_out[r["maker"]] += 1
            continue
        g1, g7, length, where = bc
        key = (r["maker"], r["calibre"], r["mass_gr"], r["velocity"]["value"], g1, g7)
        if key in loads:
            loads[key]["codes"].append(r["code"])
            continue
        calibre = CALIBRE_NAMES.get(r["calibre"], r["calibre"])
        diameter = DIAMETER[r["calibre"]]
        bullet = {"caliber": BULLET_CALIBRES[diameter], "weight_gr": r["mass_gr"], "diameter_in": diameter}
        if length:
            bullet["length_in"] = length
        if g1:
            bullet["g1"] = g1
        if g7:
            bullet["g7"] = g7
        loads[key] = {
            "name": f"{r['maker']} {calibre} {r['mass_gr']:g} gr {describe(r)}".strip(),
            "manufacturer": r["maker"],
            "caliber": calibre,
            "codes": [r["code"]] if r["code"] else [],
            "bullet": bullet,
            "muzzle_velocity": f"{r['velocity']['value']:g} {r['velocity']['unit']}",
            "barrel_length": f"{r['barrel']['value']:g} {r['barrel']['unit']}" if r["barrel"] else "",
            "reference": f"{r['source']} (checked {r['checked']}); BC: {where}, {r['bc_note']}",
        }
    out = sorted(loads.values(), key=lambda c: (c["manufacturer"], c["caliber"], c["bullet"]["weight_gr"], c["name"]))
    # Names identify records: two loads that read the same get their codes.
    names = collections.Counter(c["name"] for c in out)
    for c in out:
        if names[c["name"]] > 1 and c["codes"]:
            c["name"] += f" ({c['codes'][0]})"
    names = collections.Counter(c["name"] for c in out)
    assert all(n == 1 for n in names.values()), [k for k, n in names.items() if n > 1]
    doc = {
        "format": "balcalc-cartridges",
        "version": 1,
        "note": "Factory loads with the muzzle velocity, test barrel and BC their makers publish (each "
                "load's page in reference). Muzzle velocity depends on your barrel and powder temperature: "
                "chronograph it. Collected by collect_cartridges.py from sources/cartridges_*.jsonl.",
        "cartridges": out,
    }
    text = json.dumps(doc, ensure_ascii=False, indent=2)
    text = re.sub(r"\{\n\s+(\"name\".*?)\n\s+\}(?=,?\n\s*[\]{])",
                  lambda m: "{" + re.sub(r"\n\s+", " ", m.group(1)) + "}", text, flags=re.S)
    (HERE / "published_cartridges.json").write_text(text + "\n", encoding="utf-8", newline="\n")
    print(len(out), "cartridges from", len(rows), "rows; left out (no velocity or BC):", dict(left_out))
    print(dict(collections.Counter(c["manufacturer"] for c in out)))


if __name__ == "__main__":
    main()
