"""nosler_pages.jsonl (values copied from nosler.com product pages) -> nosler.json."""

import json
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
CALIBERS = {0.224: ".224", 0.243: "6mm", 0.264: "6.5mm", 0.277: ".277", 0.284: "7mm", 0.308: ".308",
            0.323: "8mm", 0.338: ".338"}

out = []
for line in (HERE / "nosler_pages.jsonl").read_text(encoding="utf-8").splitlines():
    p = json.loads(line)
    model = re.sub(r"^[\d.]+\s*(?:mm|Caliber)\s+\d+gr\s+", "", p["title"]).strip()
    b = {"name": f"{model} {p['weight_gr']:g} gr", "manufacturer": "Nosler",
         "caliber": CALIBERS[p["diameter"]], "weight_gr": float(p["weight_gr"]), "diameter_in": p["diameter"]}
    if p.get("length_in"):
        b["length_in"] = p["length_in"]
    b["g1"] = p["g1"]
    if p.get("g7"):
        b["g7"] = p["g7"]
    b["reference"] = f"nosler.com product page, #{p['part']} (checked 2026-10)"
    out.append(b)
(HERE / "nosler.json").write_text(json.dumps({"bullets": out}, indent=1) + "\n", encoding="utf-8")
print(len(out), "Nosler bullets")
