"""Shared helpers for collecting factory cartridges from makers' pages.

Every row is copied as published (never computed) with the page it came
from, one JSON object per line in cartridges_<maker>.jsonl:

  maker, line, code, calibre (ours), calibre_published, bullet, mass_gr,
  velocity {value, unit}, barrel {value, unit} or null,
  bc_g1, bc_g7 (null when not stated), bc_note, source, checked
"""

import datetime
import html
import json
import pathlib
import re
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
TODAY = datetime.date.today().isoformat()
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130 Safari/537.36"

# The calibres collected, as makers spell them -> our name.
CALIBRES = {
    ".223 Rem": [r"\.?223 rem(ington)?\b", r"^\.?223$"],
    "5.56x45 NATO": [r"5\.56\s*(x\s*45|mm)", r"5\.56 nato"],
    ".243 Win": [r"\.?243 win(chester)?\b"],
    "6.5 Creedmoor": [r"6\.5\s*(mm\s*)?creedmoor", r"6\.5\s*cm\b"],
    "6.5x55 SE": [r"6\.5\s*x\s*55", r"6\.5\s*swedish"],
    "6.5 PRC": [r"6\.5\s*prc"],
    "7mm Rem Mag": [r"7\s*mm\s*rem(ington)?\.?\s*mag(num)?"],
    ".308 Win": [r"\.?308 win(chester)?\b", r"^\.?308$"],
    "7.62x51 NATO": [r"7\.62\s*x\s*51"],
    ".30-06": [r"\.?30-06", r"^30\.06\b"],
    ".300 Win Mag": [r"\.?300 win(chester)?\.?\s*mag(num)?"],
    ".300 PRC": [r"\.?300\s*prc"],
    ".338 Lapua Mag": [r"\.?338 lapua"],
    "7.62x54R": [r"7\.62\s*x\s*54\s*r"],
    "7.62x39": [r"7\.62\s*x\s*39"],
    "5.45x39": [r"5\.45\s*x\s*39"],
}

# "2,850" is thousands; "0,646" (a European decimal comma) is not.
THOUSANDS = re.compile(r"(?<!\d)([1-9]\d{0,2}),(\d{3})(?!\d)")
NUMBER = re.compile(r"-?(?:\d+(?:[.,]\d+)?|[.,]\d+)")


def calibre_of(text):
    """Our calibre name for a maker's spelling, or None when not collected."""
    t = text.strip().lower().replace("×", "x")  # "6.5×55": the multiplication sign
    t = re.sub(r"(\d),(\d)", r"\1.\2", t)  # "6,5x55": a decimal comma
    for ours, patterns in CALIBRES.items():
        if any(re.search(p, t) for p in patterns):
            return ours
    return None


def fetch(url):
    """A page's HTML (curl: some makers refuse Python's default client)."""
    for attempt in range(3):
        out = subprocess.run(["curl", "-sL", "--compressed", "--max-time", "60", "-A", UA, url],
                             capture_output=True)
        if out.returncode == 0:
            return out.stdout.decode("utf-8", "replace")
    print(f"skipped (curl {out.returncode}): {url}")
    return ""


def text_of(fragment):
    return html.unescape(re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", fragment))).strip()


def number(s):
    """The first number in s: "2,850" fps is 2850 (thousands), ".646" and "0,646" are 0.646."""
    s = THOUSANDS.sub(lambda m: m.group(1) + m.group(2), s or "")
    m = NUMBER.search(s)
    return float(m.group(0).replace(",", ".")) if m else None


def write(maker, rows):
    path = HERE / f"cartridges_{maker}.jsonl"
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for r in rows:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
    print(f"{path.name}: {len(rows)} rows")


def row(maker, line, code, calibre_published, bullet, mass_gr, velocity, velocity_unit, source,
        barrel=None, barrel_unit="in", bc_g1=None, bc_g7=None, bc_note="", notes=""):
    return {
        "maker": maker,
        "line": line,
        "code": code,
        "calibre": calibre_of(calibre_published),
        "calibre_published": calibre_published,
        "bullet": bullet,
        "mass_gr": mass_gr,
        "velocity": {"value": velocity, "unit": velocity_unit},
        "barrel": None if barrel is None else {"value": barrel, "unit": barrel_unit},
        "bc_g1": bc_g1,
        "bc_g7": bc_g7,
        "bc_note": bc_note,
        "notes": notes,
        "source": source,
        "checked": TODAY,
    }
