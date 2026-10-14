"""Counts for cartridges_REPORT.md, read straight from the cartridges_*.jsonl
files: loads per maker and calibre, how many have a BC (on the page or via a
matching bullet in published_bullets.json) and a test barrel length."""

import collections
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
CALIBRES = [".223 Rem", "5.56x45 NATO", ".243 Win", "6.5 Creedmoor", "6.5x55 SE", "6.5 PRC", "7mm Rem Mag",
            ".308 Win", "7.62x51 NATO", ".30-06", ".300 Win Mag", ".300 PRC", ".338 Lapua Mag",
            "7.62x54R", "7.62x39", "5.45x39"]
DIAMETER = {".223 Rem": .224, "5.56x45 NATO": .224, ".243 Win": .243, "6.5 Creedmoor": .264, "6.5x55 SE": .264,
            "6.5 PRC": .264, "7mm Rem Mag": .284, ".308 Win": .308, "7.62x51 NATO": .308, ".30-06": .308,
            ".300 Win Mag": .308, ".300 PRC": .308, ".338 Lapua Mag": .338}
# Nosler ammunition bullet names -> words in the bullet library's names.
NOSLER_BULLETS = {"accubond long range": "accubond long range", "accubond": "accubond", "partition": "partition",
                  "ballistic tip": "ballistic tip", "custom competition": "custom competition", "rdf": "rdf",
                  "expansion tip": "e-tip", "varmageddon": "varmageddon"}


def load(path):
    return [json.loads(line) for line in open(path, encoding="utf-8")]


def library_bullet(r, library):
    """The Nosler library bullet this Nosler load uses, by diameter, weight and name."""
    if r["maker"] != "Nosler" or r["calibre"] not in DIAMETER:
        return None
    name = (r["line"] + " " + r["bullet"]).lower()
    key = next((v for k, v in NOSLER_BULLETS.items() if k in name), None)
    for b in library:
        if (b["manufacturer"] == "Nosler" and abs(b["diameter_in"] - DIAMETER[r["calibre"]]) < 0.002
                and b["weight_gr"] == r["mass_gr"] and key and key in b["name"].lower()):
            return b
    return None


def main():
    library = json.load(open(HERE.parent / "published_bullets.json", encoding="utf-8"))["bullets"]
    rows = []
    for path in sorted(HERE.glob("cartridges_*.jsonl")):
        rows += load(path)
    makers = sorted({r["maker"] for r in rows}, key=lambda m: -sum(r["maker"] == m for r in rows))
    cells = collections.Counter((r["maker"], r["calibre"]) for r in rows)
    used = [c for c in CALIBRES if any(r["calibre"] == c for r in rows)]
    short = {c: c.replace(" Rem Mag", "RM").replace(" Win Mag", "WM").replace(" Creedmoor", "CM")
                 .replace(" Lapua Mag", "LM").replace(" NATO", "").replace(" Win", "").replace(" Rem", "")
             for c in used}

    print("| Maker | " + " | ".join(short[c] for c in used) + " | Total |")
    print("|---|" + "---:|" * (len(used) + 1))
    for m in makers:
        print(f"| {m} | " + " | ".join(str(cells[(m, c)] or "") for c in used)
              + f" | **{sum(cells[(m, c)] for c in used)}** |")
    print("| **Total** | " + " | ".join(f"**{sum(r['calibre'] == c for r in rows)}**" for c in used)
          + f" | **{len(rows)}** |")
    print()
    print("| Maker | Loads | MV | BC on page | of them G7 | BC via library bullet | No BC | Test barrel |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|")
    for m in makers:
        mine = [r for r in rows if r["maker"] == m]
        page_bc = [r for r in mine if r["bc_g1"] or r["bc_g7"]]
        via = [r for r in mine if not (r["bc_g1"] or r["bc_g7"]) and library_bullet(r, library)]
        print(f"| {m} | {len(mine)} | {sum(bool(r['velocity']['value']) for r in mine)} | {len(page_bc)} | "
              f"{sum(bool(r['bc_g7']) for r in mine)} | {len(via)} | {len(mine) - len(page_bc) - len(via)} | "
              f"{sum(bool(r['barrel']) for r in mine)} |")
    usable = [r for r in rows if r["velocity"]["value"] and (r["bc_g1"] or r["bc_g7"] or library_bullet(r, library))]
    print()
    print(f"Usable now (muzzle velocity and a BC): {len(usable)} of {len(rows)}")


if __name__ == "__main__":
    main()
