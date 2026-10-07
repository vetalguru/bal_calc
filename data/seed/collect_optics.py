"""Builds published_scopes.json and published_rifles.json: scopes and rifles
as their makers publish them, for "Choose from the library" in the rifle
editor.

    python3 collect_optics.py

Inputs (collected by hand from the makers' product pages, see README.md):

* sources/scopes_*.jsonl: one scope model per line - maker, model,
  magnification ("min", "max"), focal plane ("plane": ffp | sfp), the click
  values offered ("clicks": "mrad0.1,moa0.25", units as the rifle form takes
  them), optional "sfpTrue" (the magnification an SFP reticle is true at,
  when the maker states it), "elevation", "reticles", "note" and "source";
* sources/rifles.psv: one published variant per line,
  maker|model|calibre|barrel inches|twist inches|source. Variants that differ
  only in barrel length are merged (the twist is what the solver uses).
"""

import json
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
SOURCES = HERE / "sources"
CLICK = re.compile(r"^(mrad|moa|smoa|cm100m)(\d+(?:\.\d+)?)$")


def scopes():
    out = []
    for path in sorted(SOURCES.glob("scopes_*.jsonl")):
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            s = json.loads(line)
            clicks = []
            for c in s["clicks"].split(","):
                m = CLICK.match(c.strip())
                if not m:
                    raise SystemExit(f"{path.name}: bad click value {c!r} in {s['model']}")
                clicks.append({"units": m.group(1), "value": float(m.group(2))})
            item = {
                "maker": s["maker"],
                "model": s["model"],
                "minMagnification": float(s["min"]),
                "maxMagnification": float(s["max"]),
                "focalPlane": s["plane"],
                "clicks": clicks,
                "reticles": s.get("reticles", []),
                "source": s["source"],
            }
            if "sfpTrue" in s:
                item["sfpReferenceMagnification"] = float(s["sfpTrue"])
            for key in ("elevation", "note"):
                if key in s:
                    item[key] = s[key]
            out.append(item)
    out.sort(key=lambda s: (s["maker"], s["model"]))
    return out


def rifles():
    merged = {}
    for line in (SOURCES / "rifles.psv").read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        maker, model, caliber, barrel, twist, source = line.split("|")
        key = (maker, model, caliber, float(twist))
        r = merged.setdefault(key, {
            "maker": maker, "model": model, "caliber": caliber,
            "twistIn": float(twist), "barrelsIn": [], "source": source,
        })
        if float(barrel) not in r["barrelsIn"]:
            r["barrelsIn"].append(float(barrel))
    out = list(merged.values())
    for r in out:
        r["barrelsIn"].sort()
    out.sort(key=lambda r: (r["maker"], r["model"], r["caliber"], r["twistIn"]))
    return out


def write(name, doc):
    text = json.dumps(doc, ensure_ascii=False, indent=1)
    (HERE / name).write_text(text + "\n", encoding="utf-8")


def main():
    s = scopes()
    r = rifles()
    write("published_scopes.json", {"format": "balcalc-scopes", "version": 1, "scopes": s})
    write("published_rifles.json", {"format": "balcalc-rifles", "version": 1, "rifles": r})
    print(f"{len(s)} scopes, {len(r)} rifles")


if __name__ == "__main__":
    main()
