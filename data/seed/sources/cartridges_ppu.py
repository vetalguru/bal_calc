"""PPU (Prvi Partizan) rifle ammunition. The catalogue is a calibre search
(search_r.php, a read-only POST of the dropdown's value) listing Ctg Art.,
Bullet Art., Bullet Type, weight in g and gr and "Barrel Lenght (mm)"; each
load's "Ballistics" page (search_rb.php?id=A-363) gives velocities in m/s
from 0 to 300 m. PPU publishes no BC."""

import re
import subprocess

from cartridges_common import UA, calibre_of, fetch, number, row, text_of, write

BASE = "https://www.prvipartizan.com/"
ROW = re.compile(r"<tr\s*>" + r"\s*<td[^>]*>([^<]*)</td>" * 6 + r"\s*<td[^>]*>\s*<strong>\s*<a href=search_rb\.php\?id=([^ >]+)")


def search(calibre):
    out = subprocess.run(["curl", "-s", "--compressed", "--max-time", "60", "-A", UA,
                          "--data-urlencode", "searchterm=" + calibre, "--data", "searchtype=calibre",
                          "--data", "submit=Search", BASE + "search_r.php"], capture_output=True)
    return out.stdout.decode("utf-8", "replace")


def main():
    options = re.findall(r"<option>([^<]+)</option>", fetch(BASE + "rifle_ammo.php"))
    rows = []
    for published in options:
        published = published.strip()
        if not calibre_of(published):
            continue
        for ctg, bullet_art, kind, grams, grains, barrel, ballistics_id in ROW.findall(search(published)):
            url = BASE + "search_rb.php?id=" + ballistics_id
            velocity = re.search(r"Velocity \[m/s\]\s+(\d+)", text_of(fetch(url)))
            rows.append(row(
                "PPU", "", ctg.strip(), published, kind.strip(), number(grains),
                number(velocity.group(1)) if velocity else None, "m/s", url,
                barrel=number(barrel), barrel_unit="mm",
                bc_note="not published (velocities to 300 m are on the source page)",
                notes=f"bullet {bullet_art.strip()}, {grams.strip()} g",
            ))
    write("ppu", rows)


if __name__ == "__main__":
    main()
