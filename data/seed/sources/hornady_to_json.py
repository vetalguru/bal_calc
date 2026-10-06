"""hornady_bc_page.txt (the tables of hornady.com/BC, copied) -> hornady.json:
G7 bands at Mach 2.25 / 2.0 / 1.75 (as ft/s at the standard 1116.45 ft/s
speed of sound, the conversion the core uses for BC bands)."""

import json
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
DIAMETERS = {"22 Cal": (".224", 0.224), "6mm": ("6mm", 0.243), "25 Cal": (".257", 0.257),
             "6.5mm": ("6.5mm", 0.264), "270 Cal": (".277", 0.277), "7mm": ("7mm", 0.284),
             "30 Cal": (".308", 0.308), "338 Cal": (".338", 0.338), "375 Cal": (".375", 0.375),
             "416 Cal": (".416", 0.416)}
MACHS = (2.25, 2.0, 1.75)
SOUND_FPS = 1116.45

out = []
for line in (HERE / "hornady_bc_page.txt").read_text(encoding="utf-8").splitlines():
    if not line.strip() or line.startswith("#"):
        continue
    name, *cols = [c.strip() for c in line.split("|")]
    m = re.match(r"(?P<cal>\d+(?:\.\d+)? ?(?:Cal|mm)) (?P<wt>\d+) gr\. (?P<model>.+)$", name)
    caliber, dia = DIAMETERS[m["cal"]]
    pairs = [tuple(float(x) for x in c.split("/")) for c in cols]
    twist = re.search(r"\(1 in ([\d.]+)\" Twist\)", m["model"])
    model = re.sub(r"\s*\(.*\)", "", m["model"])
    out.append({
        "name": f"{model} {m['wt']} gr" + (f" (1:{twist[1]} twist)" if twist else ""),
        "manufacturer": "Hornady", "caliber": caliber, "weight_gr": float(m["wt"]), "diameter_in": dia,
        "g1": pairs[1][0], "g7": pairs[1][1],
        "g7_bands": [[round(mach * SOUND_FPS), g7] for mach, (_, g7) in zip(MACHS, pairs)],
        "reference": "hornady.com/BC, Doppler BCs at Mach 2.25/2.0/1.75 (checked 2026-10)",
    })
(HERE / "hornady.json").write_text(json.dumps({"bullets": out}, indent=1) + "\n", encoding="utf-8")
print(len(out), "Hornady bullets")
