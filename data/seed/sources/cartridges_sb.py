"""Sellier & Bellot rifle cartridges: every product detail page linked from the
rifle and law-enforcement listings. A detail page's "Properties" list Bullet
Type, "Weight grs 55", "Ballistic coefficient Calculation G1 0.259 G7 0.127",
"Test barrel length mm 600", and its table starts "Velocity m/s 947 ...".
The site splits numbers into spans ("0 .259", "1 060"), joined back here."""

import concurrent.futures
import re

from cartridges_common import calibre_of, fetch, number, row, text_of, write

BASE = "https://www.sellier-bellot.cz"
ROOTS = ["/en/products/rifle-ammunition/", "/en/products/law-enforcement-products/"]


def plain(fragment):
    text = text_of(re.sub(r"<script.*?</script>|<style.*?</style>", "", fragment, flags=re.S))
    # "0 .259" -> "0.259", "1 060" -> "1060", "3 ,60" -> "3,60"
    text = re.sub(r"(\d) ([.,]\d)", r"\1\2", text)
    return re.sub(r"(\b\d{1,2}) (\d{3})\b", r"\1\2", text)


def detail_links():
    seen, queue, details = set(), list(ROOTS), set()
    while queue:
        path = queue.pop()
        if path in seen:
            continue
        seen.add(path)
        for link in re.findall(r'href="(/en/products/[^"#]+)"', fetch(BASE + path)):
            if "/detail/" in link:
                details.add(link)
            elif any(link.startswith(r) for r in ROOTS) and link not in seen:
                queue.append(link)
    return sorted(details)


def header(words):
    """The page head ends "<calibre> <calibre> <bullet>": the calibre printed twice."""
    for t in range(1, 4):
        for k in range(6, 0, -1):
            a, b = words[-t - 2 * k: -t - k], words[-t - k: -t]
            if len(a) == k and a == b:
                return " ".join(b), " ".join(words[-t:])
    return None, None


def collect(path):
    url = BASE + path
    text = plain(fetch(url))
    head = re.search(r"^(.*?) (V\d{6}) [\d.]+ GRS", text)
    if not head:
        return None
    published, kind_name = header(head.group(1).split())
    if not published or not calibre_of(published):
        return None
    i = text.find("Properties Bullet Type")
    props = text[i: i + 900] if i >= 0 else ""
    kind = re.search(r"Bullet Type\s+(.+?)\s+Weight", props)
    grains = re.search(r"Weight grs\s+([\d.]+)", props)
    g1 = re.search(r"G1\s+([\d.]+)", props)
    g7 = re.search(r"G7\s+([\d.]+)", props)
    barrel = re.search(r"Test barrel length mm\s+(\d+)", props)
    velocity = re.search(r"Velocity m/s\s+(\d+(?:,\d{3})?)", text)
    return row(
        "Sellier & Bellot", path.split("/")[-4].replace("-", " "), head.group(2),
        published, kind.group(1) if kind else kind_name,
        float(grains.group(1)) if grains else None,
        number(velocity.group(1)) if velocity else None, "m/s", url,
        barrel=float(barrel.group(1)) if barrel else None, barrel_unit="mm",
        bc_g1=float(g1.group(1)) if g1 else None, bc_g7=float(g7.group(1)) if g7 else None,
        bc_note="G1 and G7 as labelled" if (g1 or g7) else "not stated",
    )


def main():
    links = detail_links()
    print(len(links), "detail pages")
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        rows = [r for r in pool.map(collect, links) if r]
    rows.sort(key=lambda r: (r["calibre"], r["code"]))
    write("sb", rows)


if __name__ == "__main__":
    main()
