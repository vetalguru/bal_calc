"""Adds Nosler rifle bullets missing from nosler_pages.jsonl, read from every
bullet product page in nosler.com's sitemap. A bullet page's spec table has
"Product Name", "BC G1", "BC G7" (some), "Overall Length (OAL) (in.)",
"Manufacturer SKU", "Type Bullet", "Bullet Weight 125gr" and "Caliber 30".
Then: python nosler_to_json.py && cd .. && python collect_bullets.py sources/nosler.json"""

import concurrent.futures
import html
import json
import pathlib
import re

from cartridges_common import fetch, number

HERE = pathlib.Path(__file__).resolve().parent
PAGES = HERE / "nosler_pages.jsonl"
# The spec table's "Caliber" -> bullet diameter (the calibres nosler_to_json.py names).
DIAMETERS = {"22": 0.224, "6mm": 0.243, "6.5mm": 0.264, "270": 0.277, "7mm": 0.284, "30": 0.308,
             "8mm": 0.323, "338": 0.338}
SPEC = re.compile(r'<th class="col label" scope="row">([^<]+)</th><td class="col data"[^>]*>([^<]*)</td>')


def collect(url):
    spec = {k.strip(): html.unescape(v).strip() for k, v in SPEC.findall(fetch(url))}
    if spec.get("Type") != "Bullet" or spec.get("Caliber") not in DIAMETERS or not spec.get("BC G1"):
        return None
    title = re.sub(r"\s*\(\d+ct\)$", "", spec.get("Product Name", ""))
    page = {"url": url, "title": title, "part": spec.get("Manufacturer SKU", ""),
            "diameter": DIAMETERS[spec["Caliber"]], "weight_gr": number(spec.get("Bullet Weight")),
            "length_in": number(spec.get("Overall Length (OAL) (in.)")), "g1": number(spec["BC G1"])}
    if spec.get("BC G7"):
        page["g7"] = number(spec["BC G7"])
    return page


def main():
    lines = PAGES.read_text(encoding="utf-8").splitlines()
    known = {json.loads(line)["part"] for line in lines}
    urls = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch("https://www.nosler.com/sitemap.xml"))
            if re.search(r"-\d+gr-.*-\d+ct\.html$", u)]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        found = [p for p in pool.map(collect, urls) if p and p["part"] not in known]
    # One bullet per part (the same bullet is sold in several box sizes).
    added = {p["part"]: p for p in found}
    lines += [json.dumps(p) for p in sorted(added.values(), key=lambda p: (p["diameter"], p["weight_gr"], p["title"]))]
    PAGES.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(len(added), "bullets added;", len(lines), "in", PAGES.name)


if __name__ == "__main__":
    main()
