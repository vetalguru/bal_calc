"""Norma cartridges: every hunting and shooting product from the products
sitemap. The page head lists "Prod. No", the title (line, calibre, mass),
"Bullet Description", "Ballistic Coefficient 0.384 G1", "Bullet Weight 10 g /
155 gr", "Velocity 810 m/s / 2657 f/s" and "zeroed with a 610 mm / 24 in
barrel"."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

SITEMAP = "https://www.norma-ammunition.com/products/sitemap/sitemap-products-en.xml"


def collect(url):
    page = fetch(url)
    title = re.search(r"<title>([^<|]+)", page)
    if not title:
        return None
    title = text_of(title.group(1))
    if not calibre_of(title):
        return None
    text = text_of(re.sub(r"<script.*?</script>|<style.*?</style>", "", page, flags=re.S))
    i = text.find("Prod. No")
    head = text[i: i + 600] if i >= 0 else ""
    code = re.search(r"Prod\. No:\s*(\S+)", head)
    bullet = re.search(r"Bullet Description\s+(.+?)\s+(?:Ballistic Coefficient|Bullet Weight)", head)
    bcs = dict((g.upper(), float(v)) for v, g in re.findall(r"Ballistic Coefficient\s+([\d.]+)\s*(G\d)", head))
    gr = re.search(r"Bullet Weight\s+[\d.,]+\s*g\s*/\s*([\d.,]+)\s*gr", head)
    mps = re.search(r"Velocity\s+([\d.,]+)\s*m/s", head)
    barrel = re.search(r"zeroed with a\s+(\d+)\s*mm", head)
    line = url.rstrip("/").split("/")[-2].removeprefix("norma-").replace("-", " ")
    return row(
        "Norma", line, code.group(1) if code else "", title.removeprefix("Norma "),
        bullet.group(1) if bullet else "",
        number(gr.group(1)) if gr else None,
        number(mps.group(1)) if mps else None, "m/s", url,
        barrel=number(barrel.group(1)) if barrel else None, barrel_unit="mm",
        bc_g1=bcs.get("G1"), bc_g7=bcs.get("G7"),
        bc_note="as labelled on the page" if bcs else "not stated",
        notes="calibre from the page title",
    )


def main():
    urls = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP))
            if "/products/hunting/" in u or "/products/shooting/" in u]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, urls) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("norma", rows)


if __name__ == "__main__":
    main()
