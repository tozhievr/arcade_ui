# Arcade UI standards contract — draft 0.3

Normative references:
- HTML Living Standard: https://html.spec.whatwg.org/multipage/
- CSS Snapshot 2026: https://www.w3.org/TR/css/

## Scope
Arcade UI accepts **standard HTML markup and CSS syntax**, compiled ahead-of-time into `.arcuxi`. It is **not** a browser, does not run JavaScript, and does not provide DOM APIs, network navigation, legacy rendering modes, quirks mode, or browser extension APIs.

**Standards-based syntax is not full standards conformance.** The HTML Living Standard parsing and tree-construction algorithms are significantly more complex than this build-time parser. Only the explicitly supported subset is implemented; missing elements are diagnosed rather than emulated. CSS Snapshot 2026 is an umbrella of CSS specifications with different maturity, not a single monolithic grammar.

## Implemented in this revision
- HTML tags: div, span, p, b, strong, i, em, small, label, button, section, header, footer, main, nav, article, aside, ul, ol, li, h1-h6, a, img, input, br, hr, form, fieldset, legend, output, progress, details, summary, figure, figcaption; html, head, body, style, title and basic metadata are treated specially.
- Void element syntax (`img`, `input`, `br`, `hr`) without bogus closing tags.
- `<!doctype html>`, UTF-8 source text, HTML character references via Python's HTMLParser.
- CSS parsing via `tinycss2` and selectors via `cssselect2`: complex, descendant, child, attribute and supported pseudo-class selectors; author declarations and `!important`; inline styles.
- Paint/layout CSS subset: `display: block|flex|grid|none`, `flex-direction: row|column`, dimensions in px or auto, `flex-grow`, `flex-shrink`, `gap`, `padding`, `margin`, `font-size`, background and text color. The current layout algorithm does **not** implement all Flexbox/Grid rules.
- Native event names through `data-action="refresh"`, independent of JavaScript. The `data-*` naming follows HTML's global custom data attributes.
- Diagnostic on unsupported CSS declarations and at-rules. HTML scripting tags and `on*` event handler attributes are forbidden.

## Not yet conformant / absent
- WHATWG HTML5 full tokenizer/tree builder (foster parenting, implied end tags, foreign content, templates, text node order, etc.). Current parser requires balanced supported tags.
- CSS cascade layers, origins beyond author styles, inheritance, custom properties, `var()`, logical properties, functions and units beyond this subset, media/container queries, pseudo-elements, shadow DOM.
- CSS Flexbox/Grid specification algorithms (intrinsic min/max-content, wrapping, track sizing, auto placement, baseline alignment, percentages, minmax, spanning, fragmentation).
- Font shaping, bidi, text layout, GPU rasterizer, accessibility mapping and IME.
- JavaScript, `<script>`, browser navigation and runtime DOM.

## Roadmap / acceptance criteria
1. Replace the minimal HTML parser with an HTML Living Standard tokenizer/tree builder while retaining the ArcUX semantic element whitelist; add relevant html5lib/WPT fixtures.
2. Introduce typed computed CSS values and a full cascade; then intrinsic layout + Flexbox/Grid track algorithms with independent reference cases.
3. Preserve meaningful text-node order and mixed content; add style inheritance and paint order.
4. Add layout invalidation graph, frame arenas, coroutine scheduling with cancellation/lifetime safety.
5. Add image/font handling, GPU backends, accessibility and robust binary validation.

## Portability and dependencies
The AOT compiler requires Python 3.11+, `tinycss2>=1.5,<2` and `cssselect2>=0.9,<1`. These **build-time** dependencies do not enter the C++ module runtime. This revision keeps `.arcuxi` v1 layout intact so the native loader can consume existing images.
