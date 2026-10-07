"""Writes the generic reticles (reticle/generic-*.reticle): no maker's
drawing, only the subtensions that holding with a reticle needs.

    python3 make_reticles.py

Makers publish their reticle drawings as pictures, so a branded reticle
cannot be drawn exactly from published figures (plan data-libraries, B7).
These generic ones are defined by their mark spacing alone, which is what
a hold is read against: a MIL tree reticle with 0.2 mil hashes holds the
same as "MRAD tree 0.2" here, whatever its maker's line widths and shapes.

Coordinates are in the reticle's unit (mil or moa), y up, aiming point 0,0
(the importer converts to mrad).
"""

import pathlib

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE / "reticle"


def num(v):
    return f"{v:.3f}".rstrip("0").rstrip(".") if v != int(v) else str(int(v))


class Reticle:
    def __init__(self, name, unit, half_x, half_y):
        self.name, self.unit, self.hx, self.hy = name, unit, half_x, half_y
        self.items = []

    def a(self, v):
        return f'"{num(round(v, 4))}{self.unit}"'

    def line(self, x1, y1, x2, y2, w):
        self.items.append(f'<reticle-line start-x={self.a(x1)} start-y={self.a(y1)} '
                          f'end-x={self.a(x2)} end-y={self.a(y2)} line-width={self.a(w)} line-color="black" />')

    def dot(self, x, y, r):
        self.items.append(f'<reticle-circle center-x={self.a(x)} center-y={self.a(y)} radius={self.a(r)} '
                          f'fill="true" line-width={self.a(r / 4)} color="black" />')

    def text(self, x, y, h, s):
        self.items.append(f'<reticle-text position-x={self.a(x)} position-y={self.a(y)} '
                          f'text-height={self.a(h)} text={s!r} color="black" />'.replace("'", '"'))

    def xml(self):
        body = "\n".join("    " + i for i in self.items)
        return (f'<reticle name="{self.name}" size-x={self.a(2 * self.hx)} size-y={self.a(2 * self.hy)} '
                f'zero-x={self.a(self.hx)} zero-y={self.a(self.hy)}>\n  <elements>\n{body}\n  </elements>\n</reticle>\n')


def steps(step, extent):
    """step, 2*step ... up to extent (whole multiples only)."""
    n = round(extent / step)
    return [k * step for k in range(1, n + 1)]


def is_multiple(v, of):
    return abs(v / of - round(v / of)) < 1e-6


def hash_reticle(name, unit, step, extent, major, label, fine, tick):
    """Crosshair with hash marks every `step` on both axes, longer every
    `major`, numbered every `label`; thick posts beyond `extent`."""
    r = Reticle(name, unit, extent * 1.4, extent * 1.4)
    r.line(-extent, 0, extent, 0, fine)
    r.line(0, -extent, 0, extent, fine)
    for post in ((extent, 0, extent * 1.4, 0), (-extent, 0, -extent * 1.4, 0),
                 (0, extent, 0, extent * 1.4), (0, -extent, 0, -extent * 1.4)):
        r.line(*post, fine * 8)
    for v in steps(step, extent):
        h = tick * (2 if is_multiple(v, major) else 1)
        for s in (1, -1):
            r.line(s * v, -h / 2, s * v, h / 2, fine)  # on the horizontal axis
            r.line(-h / 2, s * v, h / 2, s * v, fine)  # on the vertical axis
        if is_multiple(v, label):
            r.text(v, -tick * 2.5, tick * 1.5, num(v))
            r.text(tick * 2, -v, tick * 1.5, num(v))
    return r


def tree_reticle(name, unit, step, rows, row_step, dot_step, fine, tick):
    """Hashed crosshair (every `step`) plus a tree below the centre: a row
    every `row_step` down to `rows`, each with dots every `dot_step`,
    widening as it goes down (more wind at longer range)."""
    side = rows * 0.6
    r = Reticle(name, unit, side * 1.3, rows * 1.15)
    r.line(-side, 0, side, 0, fine)
    r.line(0, -rows, 0, rows * 0.5, fine)
    for v in steps(step, side):
        h = tick * (2 if is_multiple(v, row_step) else 1)
        for s in (1, -1):
            r.line(s * v, -h / 2, s * v, h / 2, fine)
    for v in steps(step, rows * 0.5):
        h = tick * (2 if is_multiple(v, row_step) else 1)
        r.line(-h / 2, v, h / 2, v, fine)
    for v in steps(step, rows):
        if is_multiple(v, row_step):
            continue
        r.line(-tick / 2, -v, tick / 2, -v, fine)
    for y in steps(row_step, rows):
        width = min(side, dot_step * (2 + round(y / row_step)))
        r.line(-tick, -y, tick, -y, fine)
        for x in steps(dot_step, width):
            r.dot(x, -y, fine * 1.5)
            r.dot(-x, -y, fine * 1.5)
        if is_multiple(y, row_step * 2):
            r.text(-width - tick * 3, -y + tick, tick * 1.5, num(y))
    return r


def grid_reticle(name, unit, step, side, up, down, fine):
    """Fine crosshair and a dot at every `step` in both directions: `side`
    left and right, `up` above and `down` below the centre."""
    r = Reticle(name, unit, side * 1.2, max(up, down) * 1.1)
    r.line(-side, 0, side, 0, fine)
    r.line(0, -down, 0, up, fine)
    n = round(side / step)
    for i in range(-n, n + 1):
        for j in range(-round(down / step), round(up / step) + 1):
            if i and j:
                r.dot(i * step, j * step, fine * 1.5)
    for v in steps(step, side):
        for s in (1, -1):
            r.line(s * v, -fine * 6, s * v, fine * 6, fine)
    for v in steps(step, up):
        r.line(-fine * 6, v, fine * 6, v, fine)
    for v in steps(step, down):
        r.line(-fine * 6, -v, fine * 6, -v, fine)
    return r


def duplex(name, unit, gap, fine):
    """Plain crosshair with thick posts, no marks (a hunting reticle)."""
    r = Reticle(name, unit, gap * 4, gap * 4)
    r.line(-gap, 0, gap, 0, fine)
    r.line(0, -gap, 0, gap, fine)
    for post in ((gap, 0, gap * 4, 0), (-gap, 0, -gap * 4, 0), (0, gap, 0, gap * 4), (0, -gap, 0, -gap * 4)):
        r.line(*post, fine * 8)
    return r


RETICLES = {
    "generic-mrad-hash-0.5": hash_reticle("MRAD hash 0.5", "mil", 0.5, 10, 1, 2, 0.03, 0.2),
    "generic-mrad-hash-0.2": hash_reticle("MRAD hash 0.2", "mil", 0.2, 10, 1, 2, 0.02, 0.15),
    "generic-mrad-tree-0.2": tree_reticle("MRAD tree 0.2", "mil", 0.2, 10, 1, 1, 0.02, 0.15),
    "generic-mrad-tree-0.5": tree_reticle("MRAD tree 0.5", "mil", 0.5, 12, 1, 0.5, 0.03, 0.2),
    "generic-mrad-grid-0.5": grid_reticle("MRAD grid 0.5", "mil", 0.5, 6, 4, 10, 0.03),
    "generic-moa-hash-1": hash_reticle("MOA hash 1", "moa", 1, 30, 5, 10, 0.1, 0.6),
    "generic-moa-hash-2": hash_reticle("MOA hash 2", "moa", 2, 40, 10, 10, 0.12, 0.8),
    "generic-moa-tree-2": tree_reticle("MOA tree 2", "moa", 2, 40, 4, 2, 0.12, 0.6),
    "generic-moa-grid-2": grid_reticle("MOA grid 2", "moa", 2, 20, 14, 34, 0.12),
    "generic-duplex": duplex("Duplex (no marks)", "moa", 15, 0.15),
}


def main():
    for old in OUT.glob("generic-*.reticle"):
        old.unlink()
    for name, r in RETICLES.items():
        (OUT / f"{name}.reticle").write_text(r.xml(), encoding="utf-8")
    print(f"{len(RETICLES)} reticles")


if __name__ == "__main__":
    main()
