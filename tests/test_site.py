"""End-to-end checks for the QOYL homepage (index.html).

Run from the repo root:

    pip install pytest playwright pillow && playwright install chromium
    python3 -m pytest tests -q

Set AXE_PATH to a local axe-core/axe.min.js to include the accessibility audit
(`npm install axe-core` somewhere and point at it); the audit is skipped otherwise.
"""
from __future__ import annotations

import http.server
import json
import os
import re
import socket
import threading
from functools import partial
from html.parser import HTMLParser
from pathlib import Path

import pytest
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / "index.html"
HTML = INDEX.read_text()
HTML_NO_COMMENTS = re.sub(r"<!--.*?-->", "", HTML, flags=re.S)
WIDTHS = [320, 360, 390, 414, 768, 1024, 1280, 1440, 1920]


# ----------------------------------------------------------------- fixtures
@pytest.fixture(scope="session")
def server():
    """Serve the repo root on a free localhost port for the browser tests."""
    sock = socket.socket()
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    handler = partial(http.server.SimpleHTTPRequestHandler, directory=str(ROOT))
    handler.log_message = lambda *a, **k: None  # type: ignore[attr-defined]
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)
    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    yield f"http://127.0.0.1:{port}"
    httpd.shutdown()


@pytest.fixture(scope="session")
def browser():
    with sync_playwright() as pw:
        b = pw.chromium.launch()
        yield b
        b.close()


def open_page(browser, server, width=1280, height=900, **kw):
    ctx = browser.new_context(viewport={"width": width, "height": height}, **kw)
    page = ctx.new_page()
    console, failed = [], []
    page.on("console", lambda m: console.append((m.type, m.text)))
    page.on("requestfailed", lambda r: failed.append(r.url))
    page.on("response", lambda r: failed.append(f"{r.status} {r.url}") if r.status >= 400 else None)
    page.goto(f"{server}/index.html", wait_until="networkidle")
    return ctx, page, console, failed


# ----------------------------------------------------------------- static structure
class _Collector(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tags, self.ids, self.hrefs, self.headings = [], [], [], []
        self.scripts_src, self.stylesheets, self.imgs = [], [], []
        self._in_heading = None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        self.tags.append(tag)
        if a.get("id"):
            self.ids.append(a["id"])
        if tag == "a" and a.get("href"):
            self.hrefs.append(a["href"])
        if tag == "script" and a.get("src"):
            self.scripts_src.append(a["src"])
        if tag == "link" and a.get("rel") == "stylesheet":
            self.stylesheets.append(a.get("href"))
        if tag == "img":
            self.imgs.append(a)
        if tag in ("h1", "h2", "h3", "h4"):
            self._in_heading = tag
            self.headings.append([tag, ""])

    def handle_endtag(self, tag):
        if tag == self._in_heading:
            self._in_heading = None

    def handle_data(self, data):
        if self._in_heading:
            self.headings[-1][1] += data


@pytest.fixture(scope="module")
def doc():
    c = _Collector()
    c.feed(HTML_NO_COMMENTS)
    return c


def test_doctype_lang_charset_viewport():
    assert HTML.lower().startswith("<!doctype html>")
    assert '<html lang="en">' in HTML
    assert '<meta charset="utf-8">' in HTML
    assert 'name="viewport" content="width=device-width, initial-scale=1"' in HTML


def test_title_and_description_mention_the_business():
    title = re.search(r"<title>(.*?)</title>", HTML, re.S).group(1)
    assert "QOYL" in title and "Maine" in title
    desc = re.search(r'<meta name="description" content="([^"]+)"', HTML).group(1)
    assert 120 <= len(desc) <= 320
    for word in ("3D printing", "computer", "IT support", "bicycle", "drone", "qoyl@proton.me"):
        assert word.lower() in desc.lower(), word


def test_exactly_one_h1_and_sane_heading_order(doc):
    levels = [int(t[1]) for t, _ in doc.headings]
    assert levels.count(1) == 1
    assert levels[0] == 1
    for prev, nxt in zip(levels, levels[1:]):
        assert nxt <= prev + 1, "heading levels must not skip"


def test_ids_are_unique(doc):
    assert len(doc.ids) == len(set(doc.ids))


def test_nav_and_in_page_links_point_at_real_ids(doc):
    anchors = [h[1:] for h in doc.hrefs if h.startswith("#")]
    assert {"services", "for-sale", "about", "contact"} <= set(anchors)
    for a in anchors:
        assert a in doc.ids, f"#{a} has no target"


def test_every_local_link_and_asset_exists(doc):
    local = [h for h in doc.hrefs if not h.startswith(("#", "http", "mailto:"))]
    local += re.findall(r'href="((?:brand|portfolio)/[^"]+)"', HTML_NO_COMMENTS)
    assert local, "expected local links"
    for href in local:
        assert (ROOT / href.split("?")[0]).is_file(), f"missing {href}"


def test_contact_email_is_the_only_mailto(doc):
    mailtos = [h for h in doc.hrefs if h.startswith("mailto:")]
    assert mailtos
    for m in mailtos:
        assert m.split("?")[0] == "mailto:qoyl@proton.me"
    assert "lhcoyle4@gmail.com" not in HTML_NO_COMMENTS


def test_external_links(doc):
    ext = [h for h in doc.hrefs if h.startswith("http")]
    assert "https://www.ebay.com/usr/lo862370" in ext
    assert "https://github.com/lhcoyle4" in ext
    assert "https://github.com/lhcoyle4/lhcoyle4.github.io" in ext
    for h in ext:
        assert h.startswith("https://"), h


def test_old_site_and_resume_stay_reachable(doc):
    assert "core.html" in doc.hrefs
    assert "portfolio/Coyle_Louis_General_Application.pdf" in doc.hrefs


def test_no_phone_number_on_the_page():
    text = re.sub(r"<[^>]+>", " ", HTML_NO_COMMENTS)
    assert not re.search(r"\(?\b\d{3}\)?[-.\s]\d{3}[-.\s]\d{4}\b", text)


def test_no_placeholder_or_fictional_copy():
    text = re.sub(r"<[^>]+>", " ", HTML_NO_COMMENTS)
    for bad in ("Lorem", "TODO", "XXXX", "Incorporated", "New York", "555-", "qoyl.co ", "placeholder"):
        assert bad not in text, bad


def test_self_contained_no_external_scripts_fonts_or_javascript(doc):
    assert doc.scripts_src == []
    assert doc.stylesheets == []
    assert "fonts.googleapis" not in HTML and "cdn" not in HTML.lower()
    scripts = re.findall(r"<script[^>]*>", HTML)
    assert scripts == ['<script type="application/ld+json">'], "only the JSON-LD block may be a <script>"
    assert "onclick" not in HTML and "addEventListener" not in HTML


def test_json_ld_is_valid_local_business():
    raw = re.search(r'<script type="application/ld\+json">(.*?)</script>', HTML, re.S).group(1)
    data = json.loads(raw)
    assert data["@type"] == "LocalBusiness"
    assert data["name"] == "QOYL"
    assert data["email"] == "qoyl@proton.me"
    assert data["url"] == "https://qoyl.store/"
    assert data["address"]["addressLocality"] == "Cape Elizabeth"
    assert "https://www.ebay.com/usr/lo862370" in data["sameAs"]


def test_social_and_icon_metadata_files_exist():
    for meta in ('property="og:title"', 'property="og:image"', 'name="twitter:card"', 'rel="canonical" href="https://qoyl.store/"'):
        assert meta in HTML
    from PIL import Image
    og = Image.open(ROOT / "brand/og.png")
    assert og.size == (1200, 630)
    assert Image.open(ROOT / "brand/favicon-32.png").size == (32, 32)
    assert Image.open(ROOT / "brand/apple-touch-icon.png").size == (180, 180)
    assert (ROOT / "brand/favicon.svg").read_text().startswith("<svg")


def test_brand_tokens_present_and_only_brand_hex_colours_used():
    css = re.search(r"<style>(.*?)</style>", HTML, re.S).group(1)
    for tok in ("--ink:    #171A1F", "--navy:   #1C2A44", "--copper: #A9762F", "--slate:  #67707E", "--rule:   #DDE2E8"):
        assert tok in css
    hexes = {h.upper() for h in re.findall(r"#([0-9a-fA-F]{6})\b", css)}
    assert hexes <= {"171A1F", "1C2A44", "A9762F", "67707E", "DDE2E8", "FFFFFF", "B8843A"}
    assert "linear-gradient" not in css and "box-shadow" not in css, "no gradients or drop shadows — not in the identity"
    assert "letter-spacing: var(--track)" in css and "--track: 0.3em" in css


def test_mark_symbols_are_baked_in():
    assert '<symbol id="qoyl-mark" viewBox="' in HTML
    assert '<symbol id="qoyl-mark-thin" viewBox="' in HTML
    assert HTML.count('<use href="#qoyl-mark"/>') >= 3   # header, seal, contact tile
    assert HTML.count('<use href="#qoyl-mark-thin"/>') == 1
    assert 'stroke="currentColor"' in HTML and 'fill="currentColor"' in HTML


def test_services_are_all_listed(doc):
    h3s = [t.strip() for lvl, t in doc.headings if lvl == "h3"]
    assert h3s == ["3D printing", "Computer builds & repair", "IT support", "Bicycle repair", "Drone photography", "Used goods"]
    assert "QIDI Max 4" in HTML and "390 × 390 × 340 mm" in HTML


def test_for_sale_table_shape():
    table = re.search(r'<table class="stock">.*?</table>', HTML_NO_COMMENTS, re.S).group(0)
    assert len(re.findall(r"<th\b", table)) == 3
    assert 'class="empty"' in table                    # honest empty state until items are added
    # the commented template rows must carry data-labels so phones show cell headings
    template = re.search(r"<!-- Add an item.*?-->", HTML, re.S).group(0)
    assert template.count('data-label="Item"') == 3 and template.count('data-label="Price"') == 3


def test_copy_reads_plainly():
    text = re.sub(r"<[^>]+>", " ", HTML_NO_COMMENTS)
    for cliche in ("elevate", "seamless", "unlock", "cutting-edge", "passionate", "Welcome to", "state-of-the-art", "solutions"):
        assert cliche.lower() not in text.lower(), cliche


# ----------------------------------------------------------------- html-validate (optional, needs node)
def test_html_validate():
    import shutil
    import subprocess
    npx = shutil.which("npx")
    axe = os.environ.get("AXE_PATH")
    node_modules = Path(axe).parents[1] if axe else None
    if not npx or not node_modules or not (node_modules / "html-validate").exists():
        pytest.skip("html-validate not available")
    cfg = node_modules.parent / ".htmlvalidate.json"
    cfg.write_text(json.dumps({"extends": ["html-validate:recommended"], "rules": {"no-inline-style": "off", "long-title": "off", "svg-focusable": "off"}}))
    r = subprocess.run([npx, "html-validate", "--config", str(cfg), str(INDEX)], capture_output=True, text=True, cwd=str(node_modules.parent))
    assert r.returncode == 0, r.stdout + r.stderr


# ----------------------------------------------------------------- rendered behaviour
def test_loads_clean_and_all_requests_succeed(browser, server):
    ctx, page, console, failed = open_page(browser, server)
    assert failed == []
    assert [c for c in console if c[0] in ("error", "warning")] == []
    assert page.title().startswith("QOYL")
    ctx.close()


@pytest.mark.parametrize("width", WIDTHS)
def test_no_horizontal_overflow(browser, server, width):
    ctx, page, *_ = open_page(browser, server, width=width, height=900)
    assert page.evaluate("document.documentElement.scrollWidth") <= width
    # nothing pokes out to the right of the viewport either
    over = page.evaluate("""(w) => [...document.querySelectorAll('body *')]
        .filter(e => { const r = e.getBoundingClientRect(); return r.width > 0 && r.right > w + 1 && getComputedStyle(e).position !== 'fixed'; })
        .map(e => e.tagName + '.' + e.className)""", width)
    assert over == [], over
    ctx.close()


@pytest.mark.parametrize("width", [320, 390, 768, 1280, 1920])
def test_headline_sentences_stay_on_their_own_line(browser, server, width):
    ctx, page, *_ = open_page(browser, server, width=width)
    rects = page.evaluate("[...document.querySelectorAll('.hero h1 span')].map(s => s.getClientRects().length)")
    assert rects == [1, 1, 1], rects
    ctx.close()


def test_brand_marks_render_with_size(browser, server):
    ctx, page, *_ = open_page(browser, server)
    sizes = page.evaluate("""() => ({
        header: document.querySelector('.hd .mark').getBoundingClientRect().width,
        watermark: document.querySelector('.watermark').getBoundingClientRect().width,
        seal: document.querySelector('.seal').getBoundingClientRect().width,
        tile: document.querySelector('.tile .mark').getBoundingClientRect().width,
        pathLen: document.querySelector('#qoyl-mark path').getTotalLength() })""")
    assert 30 <= sizes["header"] <= 44
    assert sizes["watermark"] > 300
    assert sizes["seal"] >= 80 and sizes["tile"] >= 80
    assert 700 < sizes["pathLen"] < 850    # ~2 turns of spiral plus tail, in symbol units (R = 100)
    ctx.close()


def test_seal_text_fits_its_arcs(browser, server):
    ctx, page, *_ = open_page(browser, server)
    fits = page.evaluate("""() => [...document.querySelectorAll('.seal textPath')].map(tp => {
        const path = document.querySelector(tp.getAttribute('href'));
        return tp.parentElement.getComputedTextLength() / path.getTotalLength(); })""")
    assert len(fits) == 2 and all(0.5 < f < 0.9 for f in fits), fits
    ctx.close()


def test_sticky_header_stays_visible_after_scroll(browser, server):
    ctx, page, *_ = open_page(browser, server)
    page.evaluate("window.scrollTo(0, 2500)")
    page.wait_for_timeout(100)
    top = page.evaluate("document.querySelector('.hd').getBoundingClientRect().top")
    assert top == 0
    ctx.close()


@pytest.mark.parametrize("target", ["services", "for-sale", "about", "contact"])
def test_nav_links_scroll_to_their_section(browser, server, target):
    ctx, page, *_ = open_page(browser, server, reduced_motion="reduce")
    page.click(f'.nav a[href="#{target}"]')
    page.wait_for_timeout(150)
    assert page.url.endswith(f"#{target}")
    rect = page.evaluate(f"document.getElementById('{target}').getBoundingClientRect()")
    at_bottom = page.evaluate("Math.ceil(window.scrollY + window.innerHeight) >= document.documentElement.scrollHeight - 1")
    assert 0 <= rect["top"] <= 100 or at_bottom, rect   # lands under the sticky header (or the page ran out of scroll)
    ctx.close()


def test_skip_link_and_keyboard_focus_ring(browser, server):
    ctx, page, *_ = open_page(browser, server)
    page.keyboard.press("Tab")
    focused = page.evaluate("document.activeElement.className")
    assert focused == "skip"
    left = page.evaluate("document.querySelector('.skip').getBoundingClientRect().left")
    assert left >= 0, "skip link must become visible when focused"
    page.keyboard.press("Tab")
    outline = page.evaluate("getComputedStyle(document.activeElement).outlineStyle")
    assert outline == "solid"
    ctx.close()


def test_typography_and_colour_tokens_apply(browser, server):
    ctx, page, *_ = open_page(browser, server)
    styles = page.evaluate("""() => {
        const cs = e => getComputedStyle(document.querySelector(e));
        return { body: cs('body').color, h1: cs('h1').fontFamily, lede: cs('.lede').fontFamily,
                 wordmark: cs('.wordmark').letterSpacing, wordmarkSize: cs('.wordmark').fontSize,
                 stroke: cs('.stroke').backgroundColor, contact: cs('.contact').backgroundColor,
                 mono: cs('.ft .mono').fontFamily, btn: cs('.btn.primary').backgroundColor,
                 radius: cs('.btn').borderRadius }; }""")
    assert styles["body"] == "rgb(23, 26, 31)"
    assert "Helvetica" in styles["h1"] or "Arial" in styles["h1"]
    assert "Times" in styles["lede"]
    assert abs(float(styles["wordmark"].rstrip("px")) - 0.3 * float(styles["wordmarkSize"].rstrip("px"))) < 0.05
    assert styles["stroke"] == "rgb(169, 118, 47)"
    assert styles["contact"] == "rgb(23, 26, 31)"
    assert "Courier" in styles["mono"]
    assert styles["btn"] == "rgb(23, 26, 31)"
    assert styles["radius"] == "0px"
    ctx.close()


def test_mobile_layout_collapses_table_and_hides_tagline(browser, server):
    ctx, page, *_ = open_page(browser, server, width=390, height=844)
    st = page.evaluate("""() => {
        const cs = e => getComputedStyle(document.querySelector(e));
        return { td: cs('.stock td').display, tagline: cs('.tagline').display,
                 theadLeft: document.querySelector('.stock thead').getBoundingClientRect().left,
                 navWidth: document.querySelector('.nav').getBoundingClientRect().width,
                 vw: window.innerWidth }; }""")
    assert st["td"] == "block"
    assert st["tagline"] == "none"
    assert st["theadLeft"] < -1000
    assert st["navWidth"] >= st["vw"] - 60
    ctx.close()


def test_desktop_layout_uses_the_columns(browser, server):
    ctx, page, *_ = open_page(browser, server, width=1440)
    st = page.evaluate("""() => {
        const r = e => document.querySelector(e).getBoundingClientRect();
        return { cols: getComputedStyle(document.querySelector('.ledger li')).gridTemplateColumns.split(' ').length,
                 wmLeft: r('.watermark').left, h1Right: r('.hero h1').right,
                 aboutCols: getComputedStyle(document.querySelector('.about')).gridTemplateColumns.split(' ').length }; }""")
    assert st["cols"] == 3
    assert st["aboutCols"] == 2
    assert st["wmLeft"] > st["h1Right"], "watermark sits clear of the headline on desktop"
    ctx.close()


def test_print_and_reduced_motion_rules_exist():
    css = re.search(r"<style>(.*?)</style>", HTML, re.S).group(1)
    assert "@media print" in css
    assert "prefers-reduced-motion" in css


@pytest.mark.parametrize("width", [390, 1280])
def test_axe_accessibility_audit(browser, server, width):
    axe = os.environ.get("AXE_PATH")
    if not axe or not Path(axe).is_file():
        pytest.skip("set AXE_PATH=/path/to/axe-core/axe.min.js to run the audit")
    ctx, page, *_ = open_page(browser, server, width=width)
    page.add_script_tag(path=axe)
    result = page.evaluate("axe.run(document, {runOnly: {type: 'tag', values: ['wcag2a', 'wcag2aa', 'wcag21aa', 'best-practice']}})")
    violations = [(v["id"], v["impact"], [n["target"] for n in v["nodes"]][:5]) for v in result["violations"]]
    assert violations == [], json.dumps(violations, indent=1)
    ctx.close()


def test_screenshots_for_review(browser, server, tmp_path_factory):
    """Not an assertion so much as evidence: full-page captures at three widths."""
    out = Path(os.environ.get("SHOT_DIR", tmp_path_factory.mktemp("shots")))
    for w, h in ((390, 844), (768, 1024), (1440, 900)):
        ctx, page, *_ = open_page(browser, server, width=w, height=h)
        page.screenshot(path=str(out / f"qoyl-{w}.png"), full_page=True)
        ctx.close()
    assert len(list(out.glob("qoyl-*.png"))) == 3
