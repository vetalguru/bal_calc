"""Winchester rifle ammunition: the product listing shows calibre, bullet,
BC and muzzle velocity for every load (product pages load theirs by script).
Winchester states no test barrel length on these pages."""

import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

LISTING = "https://winchester.com/Products/Ammunition/Rifle"


def main():
    page = fetch(LISTING)
    rows = []
    for tile in page.split('class="b-producttile__more"')[1:]:
        href = re.search(r'href="([^"]+)"', tile)
        if not href or "/Products/Ammunition/Rifle/" not in href.group(1):
            continue

        def field(cls):
            m = re.search(r'b-producttile__info-item ' + cls + r'">\s*<span class="sr-only">[^<]*</span>([^<]+)<', tile)
            return text_of(m.group(1)) if m else None

        calibre = field("cartridge")
        if not calibre or not calibre_of(calibre):
            continue
        parts = href.group(1).rstrip("/").split("/")
        line, code = parts[-2].replace("-", " "), parts[-1]
        bc = number(field("coeff"))
        rows.append(row(
            "Winchester", line, code, calibre, field("type"), number(field("weight")),
            number(field("muzzle-velocity")), "fps", "https://winchester.com" + href.group(1),
            bc_g1=bc, bc_note="Winchester lists one BC, unlabelled (G1 by their convention)" if bc else "not stated",
            notes="test barrel not stated",
        ))
    write("winchester", rows)


if __name__ == "__main__":
    main()
