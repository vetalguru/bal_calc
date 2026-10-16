"""Lapua cartridges: every product from the site's WordPress API, then each
page's data block (Caliber, Bullet, Product no., Bullet weight "8.0 g (123
gr)", Muzzle velocity "850 m/s (2790 fps)", BC G1, BC G7) and the
trajectory chart's "Test barrel length [mm]"."""

import concurrent.futures
import json
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

API = "https://www.lapua.com/wp-json/wp/v2/product?per_page=100&page={}&_fields=link"


def field(text, label, until):
    m = re.search(re.escape(label) + r"\s+(.+?)\s+(?:" + until + r")(?=\s|$)", text)
    return m.group(1).strip() if m else None


def collect(url):
    text = text_of(fetch(url))
    i = text.find(" Caliber ")
    if i < 0:
        return None
    block = text[i: i + 700]
    published = field(block, "Caliber", "Bullet")
    if not published or not calibre_of(published):
        return None
    labels = "Bullet|Product no\\.|Bullet Code|Bullet type|Bullet weight|Muzzle velocity|Purpose|Twist rate|BC G1|BC G7|Primer|Description"
    bullet = field(block, "Bullet", labels)
    code = field(block, "Product no.", labels)
    bullet_code = field(block, "Bullet Code", labels)
    kind = field(block, "Bullet type", labels)
    weight = field(block, "Bullet weight", labels) or ""
    gr = re.search(r"\(([\d.,]+)\s*gr\)", weight)
    velocity = field(block, "Muzzle velocity", labels) or ""
    mps = re.search(r"([\d.,]+)\s*m/s", velocity)
    g1, g7 = number(field(block, "BC G1", labels)), number(field(block, "BC G7", labels))
    barrel = re.search(r"Test barrel length \[mm\]:\s*(\d+)", text)
    return row(
        "Lapua", bullet or "", code or "", published,
        " ".join(x for x in (bullet, bullet_code, kind) if x),
        number(gr.group(1)) if gr else None,
        number(mps.group(1)) if mps else None, "m/s", url,
        barrel=number(barrel.group(1)) if barrel else None, barrel_unit="mm",
        bc_g1=g1, bc_g7=g7, bc_note="BC G1 and BC G7 as labelled" if (g1 or g7) else "not stated",
        notes=("velocity also published as " + velocity) if velocity else "",
    )


def main():
    links = []
    for page in range(1, 10):
        try:
            batch = json.loads(fetch(API.format(page)))
        except ValueError:
            break
        if not isinstance(batch, list) or not batch:
            break
        links += [b["link"] for b in batch]
    cartridges = [l for l in links if "cartridge" in l and "case" not in l and "bullet" not in l.split("/")[-2][:10]]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, cartridges) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("lapua", rows)


if __name__ == "__main__":
    main()
