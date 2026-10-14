"""Re-checks every row of cartridges_*.jsonl against its page, independently
of the collectors' parsing: each value (bullet weight, velocity, BC, test
barrel) must be printed on the page, and the calibre must agree with the
page's title or address. Prints the rows that fail.

    python cartridges_verify.py [cache_dir]

Pages are cached in cache_dir (default: none) so a second run is quick."""

import concurrent.futures
import hashlib
import html
import json
import pathlib
import re
import sys

from cartridges_common import calibre_of, fetch

HERE = pathlib.Path(__file__).resolve().parent
CACHE = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else None


def page(url):
    if CACHE:
        f = CACHE / (hashlib.sha1(url.encode()).hexdigest() + ".html")
        if f.exists():
            return f.read_text(encoding="utf-8")
    text = fetch(url)
    if CACHE and text:
        f.write_text(text, encoding="utf-8")
    return text


def plain(raw):
    text = re.sub(r"<script(?![^>]*__NEXT_DATA__)[^>]*>.*?</script>|<style.*?</style>", " ", raw, flags=re.S)
    text = html.unescape(re.sub(r"<[^>]+>", " ", text))
    text = re.sub(r"\s+", " ", text)
    # Numbers split into spans ("0 .259", "1 060") are joined.
    text = re.sub(r"(\d) ([.,]\d)", r"\1\2", text)
    return text


def forms(value, bc=False):
    """The ways a number can be printed."""
    out = set()
    if value == int(value) and not bc:
        n = int(value)
        out |= {str(n), f"{n:,}", f"{n:,}".replace(",", " "), f"{n:,}".replace(",", ".")}
    else:
        for digits in (2, 3, 4):
            s = f"{value:.{digits}f}"
            if float(s) == round(value, digits):
                out |= {s, s.replace(".", ","), s.lstrip("0") if s.startswith("0.") else s}
        out.add(f"{value:g}")
        if bc:
            out.add(str(round(value * 1000)))  # Barnaul, Norma: "381" for 0.381
    return out


def printed(value, text, bc=False):
    for f in forms(value, bc):
        if re.search(r"(?<![\d.,])" + re.escape(f) + r"(?![\d])", text):
            return True
    return False


def calibre_hint(r, raw):
    """The calibre the page's title or address names (None if neither does)."""
    title = re.search(r"<title>([^<]+)", raw)
    slug = r["source"].rstrip("/").split("/")[-1].split("?")[0]
    slug = re.sub(r"^(\d)-(\d)-", r"\1.\2 ", slug).replace("-", " ").replace("_", " ")
    for text in (html.unescape(title.group(1)) if title else "", slug):
        c = calibre_of(text) if text else None
        if c:
            return c
    return None


SAME = [{".308 Win", "7.62x51 NATO"}, {".223 Rem", "5.56x45 NATO"}]


def check(r):
    raw = page(r["source"])
    if not raw:
        return r, ["page not fetched"]
    # The page text, then its raw source (some makers keep values in page data).
    text = plain(raw) + " | " + html.unescape(raw)
    problems = []
    if r["mass_gr"] and r["maker"] != "Sako" and not printed(r["mass_gr"], text):
        problems.append(f"weight {r['mass_gr']:g} gr not on the page")
    if r["maker"] == "Sako":
        grams = re.search(r"bullet ([\d.]+) g", r["notes"])
        if grams and not re.search(r'"bulletWeight":\s*' + re.escape(grams.group(1)) + r"\b", raw):
            problems.append(f"weight {grams.group(1)} g not in the page data")
    v = r["velocity"]["value"]
    if v and not printed(v, text) and not re.search(r'"muzzleVelocity":\s*' + str(int(v)) + r"\b", raw):
        problems.append(f"velocity {v:g} {r['velocity']['unit']} not on the page")
    for k in ("bc_g1", "bc_g7"):
        if r[k] and not printed(r[k], text, bc=True) and \
                not re.search(r'"ballisticCoefficient":\s*' + re.escape(f"{r[k]:g}"), raw):
            problems.append(f"{k} {r[k]:g} not on the page")
    if r["barrel"] and not printed(r["barrel"]["value"], text):
        problems.append(f"barrel {r['barrel']['value']:g} {r['barrel']['unit']} not on the page")
    hint = calibre_hint(r, raw)
    if hint and hint != r["calibre"] and not any({hint, r["calibre"]} <= s for s in SAME):
        problems.append(f"calibre {r['calibre']} but the title/address says {hint}")
    return r, problems


def main():
    if CACHE:
        CACHE.mkdir(parents=True, exist_ok=True)
    rows = [json.loads(line) for path in sorted(HERE.glob("cartridges_*.jsonl"))
            for line in path.read_text(encoding="utf-8").splitlines()]
    with concurrent.futures.ThreadPoolExecutor(6) as pool:
        results = list(pool.map(check, rows))
    bad = [(r, p) for r, p in results if p]
    for r, p in bad:
        print(f"{r['maker']} {r['code']} {r['calibre']} {r['mass_gr']}: {'; '.join(p)}  <{r['source']}>")
    print(f"\n{len(rows)} rows checked, {len(bad)} with problems")


if __name__ == "__main__":
    main()
