"""Hornady rifle ammunition: every product page from the sitemap. Each page
embeds its data as JSON (calibre, item number, BCs with their drag model)
and prints a ballistics table headed by the test barrel ("Test Barrel
(24")") whose first velocity is the muzzle velocity."""

import concurrent.futures
import json
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

SITEMAP = "https://www.hornady.com/sitemap.xml"


def json_field(page, key):
    m = re.search(r'"' + key + r'":("(?:[^"\\]|\\.)*"|[-\d.]+|null)', page)
    if not m:
        return None
    v = json.loads(m.group(1))
    return text_of(v) if isinstance(v, str) else v


def collect(url):
    page = fetch(url)
    published = json_field(page, "cartridgename")
    if not published or not calibre_of(published):
        return None
    text = text_of(page)
    title = json_field(page, "title") or ""
    barrel = re.search(r'Test Barrel \((\d+(?:\.\d+)?)"\)', text)
    velocity = re.search(r"Trajectory \(inches\) (\d{3,4})\b", text)
    bcs = {}
    for value_key, type_key in (("ball_coef_1", "ball_coef_type_1"), ("ball_coef_2_f", "ball_coef_type_2")):
        value, kind = number(json_field(page, value_key)), json_field(page, type_key) or ""
        if value:
            bcs[kind.strip("()")] = value
    mass = re.search(r"(\d+(?:\.\d+)?)\s*gr\b", title)
    bullet = json_field(page, "bullettitle") or ""
    line = re.sub(r"^.*?\d+\s*gr\s*", "", title).strip()
    barrel_value = float(barrel.group(1)) if barrel else None
    # Some International pages print metric values under the imperial labels
    # (8114: "Test Barrel (61")", "Velocity (FPS) 721": 61 cm, 721 m/s).
    metric = barrel_value is not None and barrel_value > 40
    return row(
        "Hornady", line, str(json_field(page, "sku") or ""), published, bullet,
        float(mass.group(1)) if mass else None,
        float(velocity.group(1)) if velocity else None, "m/s" if metric else "fps", url,
        barrel=barrel_value * 10 if metric else barrel_value, barrel_unit="mm" if metric else "in",
        bc_g1=bcs.get("G1"), bc_g7=bcs.get("G7"),
        bc_note="as labelled on the page" if bcs else "not stated",
        notes=("the page prints cm and m/s under the inch and FPS labels" if metric
               else "" if barrel else "test barrel not stated"),
    )


def main():
    urls = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP)) if "/ammunition/rifle/" in u]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, urls) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("hornady", rows)


if __name__ == "__main__":
    main()
