# qoyl.store

The homepage of **QOYL**, a one-person technical shop in Cape Elizabeth, Maine:
3D printing, computer builds and repair, IT support, bicycle repair, drone
photography, and refurbished equipment for sale. Served by GitHub Pages at
**[qoyl.store](https://qoyl.store/)** (the `CNAME` file) and mirrored at
[lhcoyle4.github.io](https://lhcoyle4.github.io/).

## Layout

| Path | What it is |
| --- | --- |
| `index.html` | The whole site: one self-contained page, no JavaScript, no external fonts, no trackers. |
| `brand/` | QOYL identity assets — the coil mark as SVG (ink, copper, white), favicon, Open Graph image, and the generator that draws them. |
| `brand/make_mark.py` | Draws the mark from the geometry measured off the identity specimen (an Archimedean spiral, 2.05 turns, 45° tail). |
| `brand/build_assets.py` | Regenerates the SVGs, bakes the mark into `index.html` as `<symbol>`s, and renders the PNG icons and `og.png` with Chromium. |
| `tests/` | Browser and structure tests for the page (see below). |
| `portfolio/` | Résumé and application portfolio PDFs, linked from the About section. |
| `core.html`, `index.js`, `index.wasm`, `src/`, `imgui/` | The previous site, *Systems Core* — a Dear ImGui instrument panel compiled to WebAssembly. Still boots at [/core.html](https://qoyl.store/core.html). |

## Editing the page

Everything lives in `index.html`. The identity tokens are the CSS custom
properties at the top of the `<style>` block (INK `#171A1F`, NAVY `#1C2A44`,
COPPER `#A9762F`, SLATE `#67707E`, RULE `#DDE2E8`; Helvetica for headings and
forms, Times for the letter; the wordmark is always letterspaced 30 %).

**For-sale list.** Find `<table class="stock">`. Copy one of the commented
template rows into `<tbody>`, keep the `data-label` attributes (they label the
cells on phones), and delete the `class="empty"` row once there is stock. Mark a
sold item with `class="sold"` on its `<tr>`. Extra storefront links sit in a
comment just above the table.

**The mark.** Don't hand-edit the `<symbol>` paths; change `brand/make_mark.py`
and run:

```sh
pip install playwright && playwright install chromium   # once, for the PNGs
python3 brand/build_assets.py                            # add --no-png to skip the PNGs
```

## Tests

```sh
pip install pytest playwright pillow && playwright install chromium
python3 -m pytest brand tests -q
```

`brand/test_make_mark.py` checks the mark's geometry against the specimen
measurements. `tests/test_site.py` serves the repo locally and checks the page:
structure (one `h1`, heading order, every link and asset resolves, the only
mailto is the shop address, no phone number, no placeholder copy), that it is
self-contained, JSON-LD validity, no horizontal overflow at nine widths from
320 px to 1920 px, the headline never wraps mid-sentence, nav links land on their
sections, keyboard focus and the skip link work, the brand tokens apply, and
the mobile table collapse. With `AXE_PATH` pointing at a local
`axe-core/axe.min.js` it also runs a WCAG 2.1 AA audit at phone and desktop
widths; with `html-validate` installed beside it, an HTML validation pass.

## Building the legacy Systems Core

Requires the Emscripten SDK; see `build_wasm.ps1` (Windows) or the
`em++` invocation in `.github/workflows/build.yml`, which rebuilds it on CI
whenever `src/` changes. Serve locally with `python -m http.server` and open
`core.html`.
