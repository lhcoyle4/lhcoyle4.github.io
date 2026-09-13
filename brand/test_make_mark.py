"""Unit tests for the QOYL mark generator.

Run:  python3 -m pytest brand/test_make_mark.py -q
"""
import math
import re
import xml.etree.ElementTree as ET

import make_mark as mm

SPEC = mm.MarkSpec(R=100.0)


def _dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def test_core_starts_at_a_over_r():
    pts = mm.spiral_points(SPEC, 0, 0)
    assert math.isclose(_dist(pts[0], (0, 0)), mm.A * SPEC.R, rel_tol=1e-9)


def test_spiral_ends_on_outer_radius():
    pts = mm.spiral_points(SPEC, 0, 0)
    r_end = SPEC.R * (mm.A + mm.B * mm.T_END)
    assert math.isclose(_dist(pts[-1], (0, 0)), r_end, rel_tol=1e-9)
    # the specimen normalises the end radius to 1.000 R (+/- 0.5 %)
    assert abs(r_end / SPEC.R - 1.0) < 0.005


def test_radius_grows_monotonically():
    pts = mm.spiral_points(SPEC, 0, 0)
    radii = [_dist(p, (0, 0)) for p in pts]
    assert all(b > a for a, b in zip(radii, radii[1:]))


def test_two_full_turns_plus_a_little():
    assert 2.04 < mm.TURNS < 2.06


def test_winding_is_counter_clockwise_on_screen():
    # In SVG space (y down) counter-clockwise on screen means the signed area
    # of the polyline sweep is negative.
    pts = mm.spiral_points(SPEC, 0, 0)
    area = 0.0
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        area += x1 * y2 - x2 * y1
    assert area < 0


def test_tail_exits_at_45_degrees_down_right():
    pts = mm.spiral_points(SPEC, 0, 0)
    end = pts[-1]
    tip = mm.tail_end(SPEC, 0, 0)
    ang = math.degrees(math.atan2(tip[1] - end[1], tip[0] - end[0]))
    assert math.isclose(ang, 45.0, abs_tol=0.5)
    # tail is collinear with the radius at the end of the spiral (it continues straight out)
    ang_radius = math.degrees(math.atan2(end[1], end[0]))
    assert math.isclose(ang_radius, 45.0, abs_tol=0.5)


def test_tail_length_is_a_third_of_r():
    pts = mm.spiral_points(SPEC, 0, 0)
    assert math.isclose(_dist(pts[-1], mm.tail_end(SPEC, 0, 0)), mm.TAIL * SPEC.R, rel_tol=1e-9)


def test_path_d_is_well_formed():
    d = mm.path_d(SPEC, 50, 50)
    assert d.startswith("M")
    assert d.count(" L") == SPEC.samples + 1        # samples segments + the tail
    assert re.fullmatch(r"M-?\d+(\.\d+)? -?\d+(\.\d+)?( L-?\d+(\.\d+)? -?\d+(\.\d+)?)+", d)


def test_bounds_contain_every_point_with_stroke_margin():
    minx, miny, maxx, maxy = mm.bounds(SPEC)
    half = SPEC.stroke * SPEC.R / 2
    for x, y in mm.spiral_points(SPEC, 0, 0) + [mm.tail_end(SPEC, 0, 0)]:
        assert minx + half <= x <= maxx - half
        assert miny + half <= y <= maxy - half
    # the tail tip is the rightmost point; the bottom of the last turn sits
    # within a hair of the tip's height (they are meant to align visually)
    tip = mm.tail_end(SPEC, 0, 0)
    assert math.isclose(maxx, tip[0] + half, abs_tol=1e-9)
    assert tip[1] + half <= maxy <= tip[1] + half + 0.005 * SPEC.R


def test_svg_document_parses_and_has_expected_parts():
    svg = mm.svg_document(SPEC, pad=10, title="QOYL mark")
    root = ET.fromstring(svg)
    ns = "{http://www.w3.org/2000/svg}"
    assert root.tag == ns + "svg"
    assert root.get("role") == "img"
    vb = [float(v) for v in root.get("viewBox").split()]
    assert len(vb) == 4 and vb[2] > 0 and vb[3] > 0
    tags = [c.tag for c in root]
    assert ns + "title" in tags and ns + "path" in tags and ns + "circle" in tags
    path = root.find(ns + "path")
    assert path.get("fill") == "none"
    assert path.get("stroke") == mm.INK
    assert path.get("stroke-linecap") == "round"
    circle = root.find(ns + "circle")
    assert math.isclose(float(circle.get("r")), SPEC.dot * SPEC.R, abs_tol=0.01)


def test_pad_adds_clear_space_symmetrically():
    a = [float(v) for v in ET.fromstring(mm.svg_document(SPEC, pad=0)).get("viewBox").split()]
    b = [float(v) for v in ET.fromstring(mm.svg_document(SPEC, pad=20)).get("viewBox").split()]
    assert math.isclose(b[0], a[0] - 20, abs_tol=0.01)
    assert math.isclose(b[1], a[1] - 20, abs_tol=0.01)
    assert math.isclose(b[2], a[2] + 40, abs_tol=0.01)
    assert math.isclose(b[3], a[3] + 40, abs_tol=0.01)


def test_square_tile_is_square_and_centred():
    svg = mm.square_tile(mm.MarkSpec(R=100, color=mm.WHITE), 320, mm.INK)
    root = ET.fromstring(svg)
    ns = "{http://www.w3.org/2000/svg}"
    assert root.get("viewBox") == "0 0 320 320"
    rect = root.find(ns + "rect")
    assert rect.get("fill") == mm.INK
    # the mark's bounding box centre should sit on the tile centre
    circle = root.find(ns + "circle")
    cx, cy = float(circle.get("cx")), float(circle.get("cy"))
    minx, miny, maxx, maxy = mm.bounds(mm.MarkSpec(R=100, color=mm.WHITE))
    assert math.isclose(cx + (minx + maxx) / 2, 160, abs_tol=0.05)
    assert math.isclose(cy + (miny + maxy) / 2, 160, abs_tol=0.05)


def test_only_brand_colours_are_used():
    allowed = {mm.INK, mm.COPPER, mm.WHITE, mm.RULE}
    for spec in (mm.MarkSpec(color=mm.INK), mm.MarkSpec(color=mm.COPPER), mm.MarkSpec(color=mm.WHITE)):
        svg = mm.svg_document(spec)
        for colour in re.findall(r'(?:stroke|fill)="(#[0-9A-Fa-f]{6})"', svg):
            assert colour in allowed


def test_matches_specimen_measurements():
    """Ratios measured from the QF-000 vectors (see make_mark.py docstring)."""
    assert math.isclose(mm.A, 0.1396, abs_tol=0.001)
    assert math.isclose(mm.B, 0.06675, abs_tol=0.0002)
    assert math.isclose(mm.TAIL, 0.340, abs_tol=0.002)
    # end angle on screen: 45 degrees below the horizontal, to the right
    end_ang = (mm.START_DEG - math.degrees(mm.T_END)) % 360
    assert math.isclose(end_ang, 45.0, abs_tol=0.5)
