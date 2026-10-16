"""Sako ammunition: each product line's page (from the sitemap) carries its
loads in the Next.js page data as "variants": {"ballisticCoefficient": 0.526,
"muzzleVelocity": 860, "bulletWeight": 8.8, "caliber": "6_5_creedmoor",
"pimId": "C663160HSA10", ...}. Velocity is m/s, weight grams. The BC is not
labelled G1 or G7, and no test barrel is shown."""

import json
import re

from cartridges_common import calibre_of, fetch, row, write

SITEMAP = "https://www.sako.global/sitemap.xml"
GRAMS_PER_GRAIN = 0.06479891


def variants(node):
    if isinstance(node, dict):
        if "muzzleVelocity" in node and "caliber" in node:
            yield node
        for v in node.values():
            yield from variants(v)
    elif isinstance(node, list):
        for v in node:
            yield from variants(v)


def main():
    lines = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP))
             if re.fullmatch(r"https://www\.sako\.global/ammunition/[\w-]+", u)]
    rows, seen = [], set()
    for url in lines:
        line = url.rsplit("/", 1)[-1].replace("-", " ")
        data = re.search(r'<script id="__NEXT_DATA__"[^>]*>(.*?)</script>', fetch(url), re.S)
        if not data:
            continue
        for v in variants(json.loads(data.group(1))):
            published = re.sub(r"(\d)_(\d)", r"\1.\2", v["caliber"]).replace("_", " ")
            code = v.get("pimId") or ""
            if not calibre_of(published) or (code, url) in seen:
                continue
            seen.add((code, url))
            grams = v.get("bulletWeight")
            bc = v.get("ballisticCoefficient")
            rows.append(row(
                "Sako", line, code, published, (v.get("bulletType") or line).replace("_", " "),
                round(grams / GRAMS_PER_GRAIN) if grams else None,
                v.get("muzzleVelocity"), "m/s", url,
                bc_g1=bc, bc_note="one BC, unlabelled (G1 assumed)" if bc else "not stated",
                notes=f"bullet {grams} g as published (grains rounded from it); "
                      f"bullet code {v.get('bulletCode', '')}; test barrel not stated",
            ))
    rows.sort(key=lambda r: (r["calibre"], r["line"], r["code"]))
    write("sako", rows)


if __name__ == "__main__":
    main()
