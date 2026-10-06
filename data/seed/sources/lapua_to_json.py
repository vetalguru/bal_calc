"""lapua_pages.jsonl (values copied from lapua.com product pages, one
page per line) -> lapua.json in the format collect_bullets.py merges."""

import json
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
CALIBERS = {0.224: ".224", 0.243: "6mm", 0.264: "6.5mm", 0.284: "7mm", 0.308: ".308", 0.310: ".308",
            0.311: ".311", 0.323: "8mm", 0.338: ".338", 0.366: "9.3mm"}

out = []
for line in (HERE / "lapua_pages.jsonl").read_text(encoding="utf-8").splitlines():
    p = json.loads(line)
    gr = float(re.search(r"\(([\d.]+)\s*gr\)", p["weight"]).group(1))
    dia = float(re.search(r"\((\.\d{3})", p["diameter"]).group(1))
    model = re.sub(r"^[\d,.]+\s*g\s*/\s*[\d.]+\s*gr\s*", "", p["title"]).strip()
    b = {"name": f"{model} {gr:g} gr", "manufacturer": "Lapua", "caliber": CALIBERS[dia],
         "weight_gr": gr, "diameter_in": dia}
    if p.get("length"):
        b["length_in"] = float(re.search(r"([\d.]+)\"", p["length"]).group(1))
    b["g1"] = float(p["g1"])
    if p.get("g7"):
        b["g7"] = float(p["g7"])
    b["reference"] = f"lapua.com product page, {p['product_no'].split(',')[0]} (checked 2026-10)"
    out.append(b)
(HERE / "lapua.json").write_text(json.dumps({"bullets": out}, indent=1) + "\n", encoding="utf-8")
print(len(out), "Lapua bullets")
