"""Berger factory ammunition: every product in the sitemap whose
"Specifications" list a muzzle velocity (bullet-only products have none):
"Part # 31040 ... Bullet Weight 140 Grain G1 BC 0.606 G7 BC 0.31 ... Muzzle
Velocity (fps) 2750 ... Cartridge 6.5mm Creedmoor", and "Performance based
on a 26″ barrel" under the trajectory table."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

SITEMAP = "https://bergerbullets.com/product-sitemap.xml"


def collect(url):
    text = text_of(re.sub(r"<script.*?</script>|<style.*?</style>", "", fetch(url), flags=re.S))
    velocity = re.search(r"Muzzle Velocity \(fps\)\s+([\d,]+)", text)
    # The last "Cartridge <name>" is the spec row (the first is the site's filter menu).
    published = (re.findall(r"Cartridge\s+(.+?)\s+(?:Bullet Weight|Product Line|Primer Type|Muzzle|G1 BC|Maximum COAL|Minimum Twist)", text) or [None])[-1]
    if not velocity or not published or not calibre_of(published):
        return None
    part = re.search(r"Part #\s+(\S+)", text)
    grains = re.search(r"Bullet Weight\s+([\d.]+)\s+Grain", text)
    g1 = re.search(r"G1 BC\s+([\d.]+)", text)
    g7 = re.search(r"G7 BC\s+([\d.]+)", text)
    barrel = re.search(r"based on a\s+([\d.]+)\s*[″\"]\s*barrel", text)
    name = url.rstrip("/").split("/")[-1]
    line = re.sub(r"^.*?\d+(?:-\d)?\s*-?(?:gr|grain)-", "", name).removesuffix("-rifle-ammunition").replace("-", " ")
    return row(
        "Berger", line, part.group(1) if part else "", published, line,
        float(grains.group(1)) if grains else None, number(velocity.group(1)), "fps", url,
        barrel=float(barrel.group(1)) if barrel else None,
        bc_g1=float(g1.group(1)) if g1 else None, bc_g7=float(g7.group(1)) if g7 else None,
        bc_note="G1 BC and G7 BC as listed" if (g1 or g7) else "not stated",
    )


def main():
    urls = re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP))
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, urls) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("berger", rows)


if __name__ == "__main__":
    main()
