#!/usr/bin/env python3
"""Bake the QOYL mark into index.html and render the raster brand assets.

  python3 brand/build_assets.py            (run from the repo root or from brand/)

1. Writes the SVG marks (make_mark.py).
2. Replaces the block between <!--QOYL-MARK-SYMBOLS:BEGIN--> and :END--> in
   index.html (or the one-shot <!--QOYL-MARK-SYMBOLS--> placeholder) with two
   <symbol>s that use currentColor, so the same path serves the header, the
   contact tile, the seal and the hero watermark.
3. Renders favicon-32.png, apple-touch-icon.png and og.png with Chromium
   (Playwright) — needed only when the mark or the palette changes.
"""
from __future__ import annotations

import asyncio
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import make_mark as mm  # noqa: E402

BEGIN = "<!--QOYL-MARK-SYMBOLS:BEGIN-->"
END = "<!--QOYL-MARK-SYMBOLS:END-->"
PLACEHOLDER = "<!--QOYL-MARK-SYMBOLS-->"


def symbol(sym_id: str, spec: mm.MarkSpec, precision: int = 1) -> str:
    minx, miny, maxx, maxy = mm.bounds(spec)
    inner = mm.svg_inner(spec, 0.0, 0.0, precision)
    inner = inner.replace(f'stroke="{spec.color}"', 'stroke="currentColor"').replace(f'fill="{spec.color}"', 'fill="currentColor"')
    return f'<symbol id="{sym_id}" viewBox="{minx:.2f} {miny:.2f} {maxx - minx:.2f} {maxy - miny:.2f}">{inner}</symbol>'


def inject(index: Path) -> None:
    html = index.read_text()
    block = BEGIN + symbol("qoyl-mark", mm.MarkSpec(R=100, stroke=0.155, dot=0.13)) \
        + symbol("qoyl-mark-thin", mm.MarkSpec(R=100, stroke=0.053, dot=0.085, samples=400)) + END
    if BEGIN in html and END in html:
        html = re.sub(re.escape(BEGIN) + ".*?" + re.escape(END), lambda _: block, html, count=1, flags=re.S)
    elif PLACEHOLDER in html:
        html = html.replace(PLACEHOLDER, block, 1)
    else:
        raise SystemExit("index.html has no QOYL-MARK-SYMBOLS placeholder")
    index.write_text(html)
    print(f"injected mark symbols into {index.relative_to(ROOT)}")


OG_HTML = """<!doctype html><html><head><meta charset="utf-8"><style>
  body{margin:0;width:1200px;height:630px;background:#171A1F;color:#fff;font-family:"Helvetica Neue",Helvetica,Arial,"Liberation Sans",sans-serif;display:flex;align-items:center;justify-content:center}
  .tile{display:flex;flex-direction:column;align-items:center;gap:30px}
  .word{font-weight:700;font-size:54px;letter-spacing:.3em;text-transform:uppercase;margin-left:.3em}
  .tag{font-size:19px;letter-spacing:.36em;text-transform:uppercase;color:#A9762F;margin-left:.36em}
  .where{margin-top:26px;font-size:16px;letter-spacing:.22em;text-transform:uppercase;color:rgba(255,255,255,.62);text-align:center;line-height:1.9;max-width:1040px}
  .where b{display:block;font-weight:400;color:rgba(255,255,255,.45)}
</style></head><body><div class="tile">
  <svg width="196" height="186" viewBox="__VB__">__MARK__</svg>
  <div class="word">Qoyl</div><div class="tag">Full potential</div>
  <div class="where">3D printing · Computers · IT support · Bicycles · Drone photography<b>Cape Elizabeth, Maine · qoyl.store</b></div>
</div></body></html>"""


async def render_pngs() -> None:
    from playwright.async_api import async_playwright
    spec = mm.MarkSpec(R=100, stroke=0.155, dot=0.13, color=mm.WHITE)
    minx, miny, maxx, maxy = mm.bounds(spec)
    og = OG_HTML.replace("__VB__", f"{minx:.2f} {miny:.2f} {maxx - minx:.2f} {maxy - miny:.2f}") \
                .replace("__MARK__", mm.svg_inner(spec, 0.0, 0.0))
    fav_svg = (HERE / "favicon.svg").read_text()
    async with async_playwright() as pw:
        browser = await pw.chromium.launch()
        page = await browser.new_page(viewport={"width": 1200, "height": 630})
        await page.set_content(og)
        await page.screenshot(path=str(HERE / "og.png"), clip={"x": 0, "y": 0, "width": 1200, "height": 630})
        for name, size in (("favicon-32.png", 32), ("apple-touch-icon.png", 180)):
            p = await browser.new_page(viewport={"width": size, "height": size})
            sized = fav_svg.replace('width="320" height="320"', f'width="{size}" height="{size}"')
            await p.set_content('<html><body style="margin:0;background:#171A1F">' + sized + "</body></html>")
            await p.screenshot(path=str(HERE / name), clip={"x": 0, "y": 0, "width": size, "height": size})
            await p.close()
        await browser.close()
    print("rendered og.png, favicon-32.png, apple-touch-icon.png")


if __name__ == "__main__":
    mm.main()
    inject(ROOT / "index.html")
    if "--no-png" not in sys.argv:
        asyncio.run(render_pngs())
