"""Collects the bundled starter library into data/seed from the research
folder (D:/Projects/ballistic_calc/Data/libs by default).

    python collect_seed.py [path/to/Data/libs]

* ammo/     - BallisticCalculator legacy cartridges (*.ammo, LGPL-2.1)
* reticle/  - BallisticCalculator reticles (*.reticle, LGPL-2.1)
* drg/      - Doppler-radar drag functions (*.drg): the readable small-arms
              curves of Exterior Ballistics' "Others"; encoded and artillery
              files are left out. Lapua's curves are not bundled: Lapua gave
              no general permission to redistribute them (users import the
              files they download from Lapua).

published_bullets.json is built by collect_bullets.py.
"""

import pathlib
import re
import shutil
import sys

HERE = pathlib.Path(__file__).resolve().parent
LIBS = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "D:/Projects/ballistic_calc/Data/libs")
BC = LIBS / "BallisticCalculator1" / "data"
EB = LIBS / "artill" / "misc" / "extra" / "Exterior Ballistics 2.5" / "Drag Functions"
SMALL_ARMS_MAX_DIAMETER_M = 0.0155  # up to 14.5 mm


def drg_header(path):
    with open(path, encoding="latin-1") as f:
        return f.readline().strip()


NUMBER = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def drg_fields(header):
    """(type, name, mass, diameter, length): the last three numbers of the
    header, whatever separates them (one source file has "x. y")."""
    kind, _, rest = header.partition(",")
    numbers = list(NUMBER.finditer(rest))
    if len(numbers) < 3:
        raise ValueError("bad header: " + header)
    first = numbers[-3]
    name = rest[: first.start()].strip().rstrip(",").strip()
    values = [float(m.group().rstrip(".")) for m in numbers[-3:]]
    return kind.strip(), name, values[0], values[1], values[2]


def safe(name):
    """File-system friendly name; no leading dot (".224 ..." would be hidden)."""
    return "".join(c if c.isalnum() or c in " .-_()" else "_" for c in name).strip().lstrip(".")


def main():
    for sub in ("ammo", "reticle", "drg"):
        target = HERE / sub
        if target.exists():
            shutil.rmtree(target)
        target.mkdir()

    # A few source files carry another cartridge's name (copy-paste). Where
    # a name repeats, files whose own name differs from it are named after
    # the file, so every cartridge survives under a meaningful name.
    ammo = sorted((BC / "legacy-ammo").rglob("*.ammo"))
    name_attr = re.compile(r'name="([^"]*)"')
    texts = {f: f.read_text(encoding="utf-8-sig") for f in ammo}
    counts = {}
    for text in texts.values():
        m = name_attr.search(text)
        if m:
            counts[m.group(1)] = counts.get(m.group(1), 0) + 1
    renamed = []
    for f in ammo:
        rel = f.relative_to(BC / "legacy-ammo")
        text = texts[f]
        m = name_attr.search(text)
        if m and counts[m.group(1)] > 1 and m.group(1) != f.stem:
            renamed.append((m.group(1), f.stem))
            text = text[: m.start(1)] + f.stem + text[m.end(1):]
        (HERE / "ammo" / safe("_".join(rel.parts))).write_text(text, encoding="utf-8")
    for old, new in renamed:
        print(f"renamed duplicate .ammo name {old!r} -> {new!r}")

    reticles = sorted((BC / "reticle").glob("*.reticle"))
    for f in reticles:
        shutil.copyfile(f, HERE / "reticle" / f.name)

    seen = set()
    drg = 0
    for f in sorted((EB / "Others").glob("*.drg")):
        header = drg_header(f)
        if "Encoded" in header:
            continue
        _, name, mass, diameter, _ = drg_fields(header)
        if diameter > SMALL_ARMS_MAX_DIAMETER_M or diameter <= 0.001 or name in seen:
            continue
        seen.add(name)
        shutil.copyfile(f, HERE / "drg" / (safe(name) + ".drg"))
        drg += 1

    shutil.copyfile(LIBS / "BallisticCalculator1" / "LICENSE", HERE / "LICENSE-BallisticCalculator.txt")
    print(f"ammo {len(ammo)}, reticle {len(reticles)}, drg {drg}")


if __name__ == "__main__":
    main()
