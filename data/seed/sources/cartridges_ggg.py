"""GGG (Giraitės ginkluotės gamykla, Lithuania) civil cartridges from the
sitemap. Each page has "Technical specification" (Caliber, Bullet type,
"Bullet weight g gr 10,89 168") and "Ballistic Properties": "Bullet mean
velocity, m/s barrel length 600 mm, twist 11" 805 ± 7" and an unlabelled
"Ballistic coefficient 0.462". The military pages state velocity at 24 m
and no BC, so they are not collected."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

SITEMAP = "https://www.ggg-ammo.lt/sitemap.xml"


def collect(url):
    text = text_of(re.sub(r"<script.*?</script>|<style.*?</style>", "", fetch(url), flags=re.S))
    i = text.find("Technical specification")
    if i < 0:
        return None
    block = text[i: i + 2500]
    published = re.search(r"Caliber\s+(.+?)\s+Bullet type", block)
    if not published or not calibre_of(published.group(1)):
        return None
    kind = re.search(r"Bullet type\s+(?:type\s+|tipas\s+)?(.+?)\s+Bullet weight", block)
    weight = re.search(r"Bullet weight\s+g\s+gr\s+([\d,.]+)\s+([\d,.]+)", block)
    velocity = re.search(r"barrel leng(?:th|ht)\s+(\d+)\s*mm.*?(\d{3,4})\s*±", block)
    bc = re.search(r"Ballistic coefficient\s+([\d.,]+)", block)
    code = re.search(r"DESIGN\s+(\w+)|DIZAINAS\s+(\w+)", text)
    return row(
        "GGG", "civil", (code.group(1) or code.group(2)) if code else "", published.group(1),
        kind.group(1) if kind else "",
        number(weight.group(2)) if weight else None,
        number(velocity.group(2)) if velocity else None, "m/s", url,
        barrel=number(velocity.group(1)) if velocity else None, barrel_unit="mm",
        bc_g1=number(bc.group(1)) if bc else None,
        bc_note="one BC, unlabelled (G1 assumed)" if bc else "not stated",
        notes="velocity is the C.I.P. test-barrel mean",
    )


def main():
    urls = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP)) if "/en/civil-ammunition/" in u]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, urls) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("ggg", rows)


if __name__ == "__main__":
    main()
