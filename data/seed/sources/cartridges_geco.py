"""Geco centrefire rifle cartridges: each line page (linked from the home
page) links its products; a product page lists "Item no: 2317804 Bullet type:
Fragmentation Bullet Weight: 10.7 g / 165 gr Barrell Length: 600 mm / 23.62
in Ballistic Coefficient: 0.404 Caliber: .308 Win." and "Velocity V0 [m/s]
825". The BC is not labelled G1 or G7."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

BASE = "https://geco-ammunition.com"
LINES = "/en/ammunition/products-overview/centrefire-rifle-cartridges/"


def collect(path):
    url = BASE + path
    text = text_of(re.sub(r"<script.*?</script>|<style.*?</style>", "", fetch(url), flags=re.S))
    published = re.search(r"Caliber:\s+(.+?)\s+Packaging unit", text)
    if not published or not calibre_of(published.group(1)):
        return None
    code = re.search(r"Item no:\s+(\S+)", text)
    kind = re.search(r"Bullet type:\s+(.+?)\s+Bullet Weight", text)
    grains = re.search(r"Bullet Weight:\s+[\d.,]+\s*g\s*/\s*([\d.,]+)\s*gr", text)
    barrel = re.search(r"Barrell? Length:\s+(\d+)\s*mm", text)
    bc = re.search(r"Ballistic Coefficient:\s+([\d.,]+)", text)
    velocity = re.search(r"Velocity V0 \[m/s\]\s+(\d+)", text)
    return row(
        "Geco", path.split("/")[-2].replace("-", " "), code.group(1) if code else "", published.group(1),
        kind.group(1) if kind else "", number(grains.group(1)) if grains else None,
        number(velocity.group(1)) if velocity else None, "m/s", url,
        barrel=number(barrel.group(1)) if barrel else None, barrel_unit="mm",
        bc_g1=number(bc.group(1)) if bc else None,
        bc_note="one BC, unlabelled (G1 assumed)" if bc else "not stated",
    )


def main():
    lines = sorted(set(re.findall(r'href="(' + LINES + r'[\w-]+)"', fetch(BASE + "/en"))))
    products = set()
    for line in lines:
        products |= set(re.findall(r'href="(' + re.escape(line) + r'/[\w-]+)"', fetch(BASE + line)))
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, sorted(products)) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("geco", rows)


if __name__ == "__main__":
    main()
