"""Federal rifle ammunition: every rifle product page from the sitemap. The
"Specs" block lists Caliber, Grain Weight, Bullet Style, Muzzle Velocity,
Ballistic Coefficient (G1) and, for some loads, Ballistic Coefficient G7.
Federal states no test barrel on these pages."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

SITEMAP = "https://www.federalpremium.com/sitemap_0.xml"
LABELS = ["Caliber", "Grain Weight", "Bullet Style", "Muzzle Velocity", "Ballistic Coefficient G7",
          "Ballistic Coefficient", "Bullet Length In", "Package Quantity", "Usage", "Ballistics"]


def specs(text):
    """Label -> value from the "Specs ... Ballistics" block."""
    i = text.find(" Specs ")
    if i < 0:
        return {}
    block = text[i + 7:]
    end = block.find(" Ballistics ")
    block = block[: end if end > 0 else 600]
    out = {}
    # Longest labels first, so "Ballistic Coefficient G7" is not read as "Ballistic Coefficient".
    pattern = "|".join(re.escape(l) for l in sorted(LABELS, key=len, reverse=True))
    parts = re.split(r"(?:^|\s)(" + pattern + r")\s", " " + block)
    for k in range(1, len(parts) - 1, 2):
        out.setdefault(parts[k], parts[k + 1].strip())
    return out


def collect(url):
    s = specs(text_of(fetch(url)))
    published = s.get("Caliber")
    if not published or not calibre_of(published):
        return None
    segments = url.split("/")
    code = re.sub(r"^\d+-", "", segments[-1].removesuffix(".html"))
    g1, g7 = number(s.get("Ballistic Coefficient")), number(s.get("Ballistic Coefficient G7"))
    return row(
        "Federal", segments[-2].replace("-", " "), code, published, s.get("Bullet Style"),
        number(s.get("Grain Weight")), number(s.get("Muzzle Velocity")), "fps", url,
        bc_g1=g1, bc_g7=g7,
        bc_note="Ballistic Coefficient (G1) and Ballistic Coefficient G7 as listed" if (g1 or g7) else "not stated",
        notes="test barrel not stated",
    )


def main():
    urls = [u for u in re.findall(r"<loc>([^<]+)</loc>", fetch(SITEMAP)) if "/rifle/" in u and u.endswith(".html")]
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, urls) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("federal", rows)


if __name__ == "__main__":
    main()
