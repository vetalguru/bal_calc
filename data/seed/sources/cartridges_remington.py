"""Remington rifle ammunition: every /rifle/ product page from the sitemap.
The page's "Specs" block is the same as Federal's (Caliber, Grain Weight,
Bullet Style, Muzzle Velocity, Ballistic Coefficient), so Federal's parser
reads it. Remington states no test barrel on these pages."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write
from cartridges_federal import specs

SITEMAP = "https://www.remington.com/sitemap_0.xml"


def collect(url):
    s = specs(text_of(fetch(url)))
    published = s.get("Caliber")
    if not published or not calibre_of(published):
        return None
    segments = url.split("/")
    g1, g7 = number(s.get("Ballistic Coefficient")), number(s.get("Ballistic Coefficient G7"))
    return row(
        "Remington", segments[-2].replace("-", " "), segments[-1].removesuffix(".html"), published,
        s.get("Bullet Style"), number(s.get("Grain Weight")), number(s.get("Muzzle Velocity")), "fps", url,
        bc_g1=g1, bc_g7=g7,
        bc_note="Ballistic Coefficient (G1) as listed" if (g1 or g7) else "not stated",
        notes="test barrel not stated",
    )


def main():
    urls = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP))
            if u.split("/")[3] == "rifle" and re.search(r"/\d+-\d+\.html$", u)]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, urls) if r]
    # The same product is listed under its line and under "shop by caliber".
    rows = list({r["code"]: r for r in sorted(rows, key=lambda r: "shop-by-caliber" in r["source"], reverse=True)}.values())
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("remington", rows)


if __name__ == "__main__":
    main()
