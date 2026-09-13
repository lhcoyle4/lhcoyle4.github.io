#!/usr/bin/env python3
"""QOYL coil mark — SVG generator.

The geometry was fitted from the vector data in the QOYL identity specimen
(QF-000, Rev A). The mark is an Archimedean spiral, wound counter-clockwise
from the core, that opens into a straight tail at 45° below the horizontal —
a wound coil (stored potential) that also reads as a Q.

    r(t) = R * (A + B * t)          t in [0, T]  (radians)
    A = 0.1396   B = 0.06675   T = 2.0503 turns
    tail: 0.340 R, continuing straight from the end of the spiral
    core dot: 0.13 R (small marks)  /  0.085 R (large watermark)
    stroke: 0.155 R (small marks)   /  0.053 R (large watermark)

Usage:  python3 make_mark.py            -> writes the SVG files next to this script
        python3 -m pytest test_make_mark.py
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

INK = "#171A1F"
COPPER = "#A9762F"
RULE = "#DDE2E8"
WHITE = "#FFFFFF"

A = 0.1396           # core radius / R
B = 0.06675          # radial growth per radian / R
TURNS = 2.0503
T_END = TURNS * 2 * math.pi
START_DEG = 63.17    # SVG-space angle (y down) of the spiral's inner end
TAIL = 0.340         # tail length / R


@dataclass(frozen=True)
class MarkSpec:
    R: float = 100.0            # outer radius of the spiral, in user units
    stroke: float = 0.155       # stroke width / R
    dot: float = 0.13           # core dot radius / R
    color: str = INK
    samples: int = 240          # polyline resolution


def spiral_points(spec: MarkSpec, cx: float, cy: float) -> list[tuple[float, float]]:
    """Points along the spiral, from the core outward."""
    pts = []
    for i in range(spec.samples + 1):
        t = T_END * i / spec.samples
        r = spec.R * (A + B * t)
        ang = math.radians(START_DEG) - t          # decreasing angle in y-down space == CCW on screen
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    return pts


def tail_end(spec: MarkSpec, cx: float, cy: float) -> tuple[float, float]:
    ang = math.radians(START_DEG) - T_END
    r = spec.R * (A + B * T_END) + spec.R * TAIL
    return (cx + r * math.cos(ang), cy + r * math.sin(ang))


def bounds(spec: MarkSpec) -> tuple[float, float, float, float]:
    """Tight bounding box (minx, miny, maxx, maxy) of the stroked mark around (0,0)."""
    pts = spiral_points(spec, 0.0, 0.0) + [tail_end(spec, 0.0, 0.0)]
    half = spec.stroke * spec.R / 2
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return (min(xs) - half, min(ys) - half, max(xs) + half, max(ys) + half)


def path_d(spec: MarkSpec, cx: float, cy: float, precision: int = 2) -> str:
    pts = spiral_points(spec, cx, cy)
    tx, ty = tail_end(spec, cx, cy)
    fmt = lambda v: f"{v:.{precision}f}".rstrip("0").rstrip(".")  # noqa: E731
    d = "M" + " L".join(f"{fmt(x)} {fmt(y)}" for x, y in pts)
    d += f" L{fmt(tx)} {fmt(ty)}"
    return d


def svg_inner(spec: MarkSpec, cx: float, cy: float, precision: int = 2) -> str:
    """The <path> + <circle> that make up the mark (no <svg> wrapper)."""
    sw = spec.stroke * spec.R
    return (
        f'<path d="{path_d(spec, cx, cy, precision)}" fill="none" stroke="{spec.color}" '
        f'stroke-width="{sw:.{precision}f}" stroke-linecap="round" stroke-linejoin="round"/>'
        f'<circle cx="{cx:.{precision}f}" cy="{cy:.{precision}f}" r="{spec.dot * spec.R:.{precision}f}" fill="{spec.color}"/>'
    )


def svg_document(spec: MarkSpec, pad: float = 0.0, background: str | None = None,
                 title: str = "QOYL", precision: int = 2) -> str:
    """A standalone SVG with a tight viewBox (plus `pad` user units of clear space)."""
    minx, miny, maxx, maxy = bounds(spec)
    minx -= pad
    miny -= pad
    maxx += pad
    maxy += pad
    w = maxx - minx
    h = maxy - miny
    bg = f'<rect x="{minx:.2f}" y="{miny:.2f}" width="{w:.2f}" height="{h:.2f}" fill="{background}"/>' if background else ""
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{minx:.2f} {miny:.2f} {w:.2f} {h:.2f}" '
        f'width="{w:.0f}" height="{h:.0f}" role="img" aria-label="{title}">'
        f"<title>{title}</title>{bg}{svg_inner(spec, 0.0, 0.0, precision)}</svg>\n"
    )


def square_tile(spec: MarkSpec, size: float, background: str, radius: float = 0.0,
                title: str = "QOYL", precision: int = 2) -> str:
    """The reversed tile from the identity sheet: mark centred on a filled square."""
    minx, miny, maxx, maxy = bounds(spec)
    cx = size / 2 - (minx + maxx) / 2
    cy = size / 2 - (miny + maxy) / 2
    rx = f' rx="{radius:.2f}"' if radius else ""
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size:.0f} {size:.0f}" '
        f'width="{size:.0f}" height="{size:.0f}" role="img" aria-label="{title}">'
        f"<title>{title}</title>"
        f'<rect width="{size:.0f}" height="{size:.0f}" fill="{background}"{rx}/>'
        f"{svg_inner(spec, cx, cy, precision)}</svg>\n"
    )


def main() -> None:
    here = Path(__file__).resolve().parent
    small = MarkSpec(R=100, stroke=0.155, dot=0.13, color=INK)
    (here / "qoyl-mark.svg").write_text(svg_document(small, pad=26, title="QOYL mark"))
    (here / "qoyl-mark-copper.svg").write_text(
        svg_document(MarkSpec(R=100, stroke=0.155, dot=0.13, color=COPPER), pad=26, title="QOYL mark"))
    (here / "qoyl-mark-white.svg").write_text(
        svg_document(MarkSpec(R=100, stroke=0.155, dot=0.13, color=WHITE), pad=26, title="QOYL mark"))
    # favicon: the reversed tile (ink square, white mark) — reads at 16 px
    fav = MarkSpec(R=100, stroke=0.17, dot=0.14, color=WHITE)
    (here / "favicon.svg").write_text(square_tile(fav, 320, INK, radius=0, title="QOYL"))
    # big watermark used on covers: thin stroke, small core
    water = MarkSpec(R=100, stroke=0.053, dot=0.085, color=RULE, samples=400)
    (here / "qoyl-watermark.svg").write_text(svg_document(water, pad=6, title=""))
    print("wrote qoyl-mark.svg, qoyl-mark-copper.svg, qoyl-mark-white.svg, favicon.svg, qoyl-watermark.svg")


if __name__ == "__main__":
    main()
