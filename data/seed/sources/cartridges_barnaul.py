"""Barnaul rifle ammunition: one page per calibre, each load a block
"FMJ 148 Grain ... Ballistic Coefficient (G1): 381 Muzzle Velocity V 0
ft/sec: 2,641". Barnaul prints the BC without its leading "0." (381 is
0.381) and states no test barrel."""

import re

from cartridges_common import fetch, number, row, text_of, write

PAGES = {
    "5.45x39": "barnaul-545x39", "5.56x45": "barnaul-556x45", "7.62x39": "barnaul-762x39",
    "7.62x51": "barnaul-762x51", "7.62x54R": "barnaul-762x54R", "30-06 (7.62x63)": "barnaul-762x63",
}
LOAD = re.compile(r"([A-Z][\w-]*(?: [A-Z][\w-]*)*) (\d+) Grain")
DATA = re.compile(r"Ballistic Coefficient \(G1\):\s*(\d+)\s+Muzzle Velocity V 0 ft/sec:\s*([\d,]+)")


def main():
    rows = []
    for published, slug in PAGES.items():
        url = f"https://www.barnaulammo.com/rifle-ammo/{slug}.php"
        text = text_of(re.sub(r"<script.*?</script>|<style.*?</style>", "", fetch(url), flags=re.S))
        start = 0
        for m in DATA.finditer(text):
            loads = list(LOAD.finditer(text, start, m.start()))
            start = m.end()
            if not loads:
                continue
            bullet, grains = loads[-1].group(1).removesuffix(" Statistics"), loads[-1].group(2)
            rows.append(row(
                "Barnaul", "Barnaul", f"{slug}-{bullet.lower().replace(' ', '-')}-{grains}", published,
                bullet, float(grains), number(m.group(2)), "fps", url,
                bc_g1=float(m.group(1)) / 1000,
                bc_note=f'G1 as labelled; printed "{m.group(1)}" without the leading 0.',
                notes="test barrel not stated",
            ))
    write("barnaul", rows)


if __name__ == "__main__":
    main()
