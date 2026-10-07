"""Builds published_bullets.json: bullets with the BCs their makers publish.

    python collect_bullets.py berger.txt [lapua.json ...]

Inputs:

* the Berger catalog as text (pdftotext of the yearly catalog PDF): every
  table row "<calibre> <weight> gr <name> <part> .<diameter>" <ogive> <G1> <G7>";
* JSON lists collected by hand from makers' product pages (same fields as
  the output, with a "reference" each), e.g. lapua.json.

Every value keeps its source in "reference". Rows whose G7/G1 ratio is not
that of a real bullet (a catalog typo such as G7 = G1) lose the suspicious
BC, and are reported.
"""

import json
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent

CALIBERS = {  # catalog calibre names -> ours (as the existing library uses)
    "17 Cal": ".172", "20 Cal": ".204", "22 Cal": ".224", "6mm": "6mm", "25 Cal": ".257",
    "6.5mm": "6.5mm", "270 Cal": ".277", "7mm": "7mm", "30 Cal": ".308", "8mm": "8mm",
    "338 Cal": ".338", "375 Cal": ".375", "416 Cal": ".416", "45 Cal": ".458", "50 Cal": ".50 BMG",
}

ROW = re.compile(
    r'^(?P<cal>\d+(?:\.\d+)?\s?(?:Cal|mm))\s+(?P<wt>[\d.]+)\s*(?:gr\s+)?(?P<name>.+?)\s+'
    r'(?P<part>\d{5})\s+(?P<dia>\.\d{3})["”]\s+(?P<ogive>\w+)\s+(?P<g1>0?\.\d{3})(?:\s+(?P<g7>0?\.\d{3})|\s+–)?\s*$')

# A real bullet's G7 BC is about 0.45..0.56 of its G1 BC.
RATIO = (0.40, 0.60)


# The catalog's specification table: part, description, calibre, diameter,
# OAL, boat tail, nose, base to ogive, bearing surface, SD, G1, G7, G7 form
# factor, minimum twist. Its BCs win over the product tables' (which carry
# typos, e.g. the .338 250 gr BCs on the .308 185 gr Juggernaut OTM Tactical).
SPEC = re.compile(
    r'^(?P<part>\d{5}) (?P<desc>.+?) (?P<dia>0\.\d{3}) (?P<oal>\d\.\d{3}) \S+ \S+ \S+ \S+ \S+ '
    r'(?P<g1>0?\.\d{3}) (?P<g7>0?\.\d{3}|N/A) \S+ (?P<twist>1:[\d.]+)')


def berger_specs(text):
    specs = {}
    for line in text.splitlines():
        m = SPEC.match(line.strip())
        if m:
            specs[m["part"]] = m
    return specs


def berger(path, problems):
    out = []
    text = pathlib.Path(path).read_text(encoding="utf-8")
    specs = berger_specs(text)
    for line in text.splitlines():
        m = ROW.match(line.strip())
        if not m:
            continue
        spec = specs.get(m["part"])
        cal = re.sub(r"\s+", " ", m["cal"]).replace("Cal", " Cal").replace("  ", " ")
        g1 = float(m["g1"])
        g7 = float(m["g7"]) if m["g7"] else None
        if spec:
            spec_g7 = None if spec["g7"] == "N/A" else float(spec["g7"])
            if (float(spec["g1"]), spec_g7) != (g1, g7):
                problems.append(f"Berger #{m['part']}: product table {g1}/{g7}, "
                                f"specification table {spec['g1']}/{spec['g7']} (used)")
            g1, g7 = float(spec["g1"]), spec_g7
        name = m["name"].strip()
        weight = float(m["wt"])
        b = {
            "name": f"{name} {weight:g} gr",
            "manufacturer": "Berger",
            "caliber": CALIBERS.get(cal, cal),
            "weight_gr": weight,
            "diameter_in": float(m["dia"]),
            **({"length_in": float(spec["oal"])} if spec else {}),
            "g1": g1,
            **({"g7": g7} if g7 else {}),
            "reference": f"Berger catalog 2025, #{m['part']}"
                         + (f", minimum twist {spec['twist']}\"" if spec else ""),
        }
        if g7 and not RATIO[0] <= g7 / g1 <= RATIO[1]:
            problems.append(f"Berger #{m['part']} {b['name']}: G7/G1 = {g7 / g1:.2f}, G7 dropped")
            del b["g7"]
        out.append(b)
    return out


def key(b):
    return (b["manufacturer"].lower(), b["caliber"], round(b["weight_gr"], 1), b["name"].lower())


def main():
    problems = []
    bullets = []
    for arg in sys.argv[1:]:
        if arg.endswith(".json"):
            bullets += json.loads(pathlib.Path(arg).read_text(encoding="utf-8"))["bullets"]
        else:
            bullets += berger(arg, problems)
    # Makers reuse model names across calibres (VLD Target 168 gr in 7mm and
    # .308): the calibre leads every name, which also reads well in a list.
    for b in bullets:
        if not b["name"].startswith(b["caliber"]):
            b["name"] = f"{b['caliber']} {b['name']}"
    # Hand-collected entries already in the library stay unless replaced.
    current = json.loads((HERE / "published_bullets.json").read_text(encoding="utf-8"))
    makers = {b["manufacturer"] for b in bullets}
    kept = [b for b in current["bullets"] if b["manufacturer"] not in makers]
    merged = {}
    for b in kept + bullets:
        merged.setdefault(key(b), b)
    out = sorted(merged.values(), key=lambda b: (b["manufacturer"], b["diameter_in"], b["weight_gr"], b["name"]))
    current["bullets"] = out
    current["note"] = current["note"].split(" Collected by collect_bullets.py")[0] + \
        " Collected by collect_bullets.py from the makers' catalogs and product pages."
    text = json.dumps(current, ensure_ascii=False, indent=2)
    # One bullet per line, as before.
    text = re.sub(r"\{\n\s+(\"name\".*?)\n\s+\}", lambda m: "{" + re.sub(r"\n\s+", " ", m.group(1)) + "}", text, flags=re.S)
    (HERE / "published_bullets.json").write_text(text + "\n", encoding="utf-8")
    print(len(out), "bullets;", len(problems), "problems")
    for p in problems:
        print("  ", p)


if __name__ == "__main__":
    main()
