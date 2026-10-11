"""Draws the Holdmark icon and writes every platform size.

    pip install pillow
    python make_icons.py

Outputs (next to this script and in ../androidApp/src/main/res):
  balcalc.png (512), balcalc.ico (16-256)          - Linux / Windows
  androidApp/src/main/res/mipmap-*/ic_launcher(_round).png      - legacy launcher icons
  androidApp/src/main/res/mipmap-*/ic_launcher_foreground.png   - adaptive icon layer
  androidApp/src/main/res/mipmap-anydpi-v26/ic_launcher*.xml    - adaptive icon
  androidApp/src/main/res/values/ic_launcher_background.xml     - adaptive background
"""

import math
import pathlib

from PIL import Image, ImageDraw

HERE = pathlib.Path(__file__).resolve().parent
RES = HERE.parent / "androidApp" / "src" / "main" / "res"

BACKGROUND = (38, 50, 56)      # Material blue grey 900
TRAJECTORY = (255, 152, 0)     # Material orange 500
CROSSHAIR = (245, 245, 245)
SUPERSAMPLE = 4


def draw_symbol(draw, size, cx, cy, scale):
    """Trajectory arc ending in a crosshair; `scale` = symbol size in px."""
    s = scale
    w = max(2, round(s * 0.055))
    # Trajectory: a ballistic arc from lower left rising and falling into the reticle.
    pts = []
    x0, x1 = cx - 0.46 * s, cx + 0.18 * s
    for i in range(60):
        t = i / 59
        x = x0 + (x1 - x0) * t
        y = cy + 0.22 * s - 0.80 * s * t + 0.70 * s * t * t
        pts.append((x, y))
    # Crosshair centred where the arc lands; drawn first, so the trajectory
    # lies on top of it.
    tx, ty = pts[-1]
    r = 0.24 * s
    draw.ellipse([tx - r, ty - r, tx + r, ty + r], outline=CROSSHAIR, width=w)
    gap = 0.07 * s
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        draw.line([(tx + dx * gap, ty + dy * gap), (tx + dx * (r + 0.10 * s), ty + dy * (r + 0.10 * s))],
                  fill=CROSSHAIR, width=max(2, w // 2 + 1))
    draw.line(pts, fill=TRAJECTORY, width=w, joint="curve")
    d = 0.035 * s
    draw.ellipse([tx - d, ty - d, tx + d, ty + d], fill=TRAJECTORY)


def centered_symbol(big, scale):
    """The symbol on a transparent layer, centred by its own bounding box."""
    layer = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw_symbol(ImageDraw.Draw(layer), big, big * 0.5, big * 0.5, scale)
    x0, y0, x1, y1 = layer.getbbox()
    out = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    out.paste(layer.crop((x0, y0, x1, y1)), ((big - (x1 - x0)) // 2, (big - (y1 - y0)) // 2))
    return out


def full_icon(px, rounded=True, circle=False):
    big = px * SUPERSAMPLE
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    if circle:
        draw.ellipse([0, 0, big - 1, big - 1], fill=BACKGROUND)
    elif rounded:
        draw.rounded_rectangle([0, 0, big - 1, big - 1], radius=big * 0.22, fill=BACKGROUND)
    else:
        draw.rectangle([0, 0, big, big], fill=BACKGROUND)
    img.alpha_composite(centered_symbol(big, big * 0.66))
    return img.resize((px, px), Image.LANCZOS)


def foreground(px):
    """Adaptive-icon foreground: symbol inside the 66 % safe zone, transparent."""
    big = px * SUPERSAMPLE
    return centered_symbol(big, big * 0.46).resize((px, px), Image.LANCZOS)


def main():
    full_icon(512).save(HERE / "balcalc.png")
    full_icon(256).save(HERE / "balcalc.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48),
                                                     (64, 64), (128, 128), (256, 256)])
    densities = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
    for name, px in densities.items():
        out = RES / f"mipmap-{name}"
        out.mkdir(parents=True, exist_ok=True)
        full_icon(px).save(out / "ic_launcher.png")
        full_icon(px, circle=True).save(out / "ic_launcher_round.png")
        foreground(px * 108 // 48).save(out / "ic_launcher_foreground.png")
    anydpi = RES / "mipmap-anydpi-v26"
    anydpi.mkdir(parents=True, exist_ok=True)
    adaptive = ('<?xml version="1.0" encoding="utf-8"?>\n'
                '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
                '    <background android:drawable="@color/ic_launcher_background"/>\n'
                '    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>\n'
                '</adaptive-icon>\n')
    for name in ("ic_launcher.xml", "ic_launcher_round.xml"):
        (anydpi / name).write_text(adaptive, encoding="utf-8", newline="\n")
    values = RES / "values"
    values.mkdir(parents=True, exist_ok=True)
    (values / "ic_launcher_background.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
        '    <color name="ic_launcher_background">#%02X%02X%02X</color>\n</resources>\n' % BACKGROUND,
        encoding="utf-8", newline="\n")
    print("icons written")


if __name__ == "__main__":
    main()
