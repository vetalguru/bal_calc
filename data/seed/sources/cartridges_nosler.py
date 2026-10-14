"""Nosler ammunition: every product page from the sitemap. The spec table
lists Manufacturer SKU, Cartridge, Bullet Type, Bullet Weight and Test Barrel
Length; the "VELOCITY (FPS) Muzzle 100 ..." table gives the muzzle velocity.
Nosler prints no BC on ammunition pages: the bullet (Nosler's own) is noted so
its BC can come from published_bullets.json."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

SITEMAP = "https://www.nosler.com/sitemap.xml"
LABELS = r"(?:Type|UPC|Proposition 65|Shipping Restrictions|Cartridge|Ammunition Use|Bullet Profile|Bullet Type|Bullet Weight|Lead Free|Test Barrel Length|Test Barrel Twist|Handgun Ammunition)"


def spec(text, label):
    m = re.search(r"\b" + label + r"\s+(.+?)\s+" + LABELS + r"\b", text)
    return m.group(1).strip() if m else None


def collect(url):
    text = text_of(re.sub(r"<script.*?</script>|<style.*?</style>", "", fetch(url), flags=re.S))
    i = text.find("Manufacturer SKU")
    if i < 0:
        return None
    block = text[i: i + 600]
    published = spec(block, "Cartridge")
    if not published or not calibre_of(published):
        return None
    sku = re.search(r"Manufacturer SKU\s+(\S+)", block)
    barrel = re.search(r'Test Barrel Length\s+([\d.]+)"', block)
    velocity = re.search(r"VELOCITY \(FPS\) Muzzle(?: \d+)+? (\d{4})\b", text)
    bullet = spec(block, "Bullet Type") or ""
    title = re.search(r"You're reviewing:\s+(.+?)\s+Your Rating", text)
    line = re.search(r"\d+gr\s+(.+?)\s+Ammunition", title.group(1)) if title else None
    return row(
        "Nosler", line.group(1) if line else bullet, sku.group(1) if sku else "", published, bullet,
        number(spec(block, "Bullet Weight")),
        number(velocity.group(1)) if velocity else None, "fps", url,
        barrel=number(barrel.group(1)) if barrel else None,
        bc_note="not on the ammunition page; the bullet is Nosler's own (see published_bullets.json)",
        notes="muzzle velocity from the page's velocity table",
    )


def main():
    urls = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP))
            if u.endswith("-ammunition.html") and "/products/" not in u]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, urls) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("nosler", rows)


if __name__ == "__main__":
    main()
