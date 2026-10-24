# Arcade UI — AOT prototype 1.3

Native C++ module runtime and deterministic HTML/CSS to `.arcuxi` compiler. Based on the original ArcadeUI skeleton.

## Build (Clang 23.1.3 + CMake 4.5.0-rc1 + Ninja)

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
./build/debug/arcadeui ./build/debug/dashboard.arcuxi
```

Install AOT Python dependencies: `python -m pip install -r tools/requirements.txt`.

AOT standalone compilation: `python tools/arcuxc.py examples/dashboard.html -o dashboard.arcuxi`.

## Implemented

- HTML subset parser, void elements, `<style>` blocks, and native `data-action` event IDs; `script` and inline JS event attributes rejected.
- CSS tokenization using tinycss2 and complex selectors using cssselect2; author cascade with specificity and `!important`; numeric px values, limited Flex row/column and basic Grid columns.
- Deterministic little-endian binary v2, relative string offsets, SHA-256 payload checksum from compiler, packed assets.
- Breadth-first densely-packed topology (contiguous siblings), PMR storage, native module, layout-to-draw batch.
- Callback ID dispatch via `function pointer + context`, hit testing, responsive recomputation via `resize`.
- Python compiler unit tests (10 cases) and runtime smoke test.

## Limitations / next stage

Not a full browser engine: no complex CSS selectors, box sizing variations, `minmax`, intrinsic Grid tracks, CSS percentages, wrapping Flex, accessibility tree, font shaping, animations, or GPU renderer yet. Currently the runtime generates draw commands and can export an SVG preview; no window or GPU rendering. Assets are packed but aren't decoded/rendered. Coroutine input scheduler and dependency-based incremental layout are future work. SHA-256 written by compiler is not yet revalidated at runtime. Format isn't hardened for untrusted images: validate further before loading external content. Text width is approximated. Grid uses explicit equal-fraction columns. `display:none` descendants are omitted from drawing; event/hit-testing behavior remains limited. Intended as foundation, not CSS specification conformance.

The original `src/app.cppm` and `src/contracts.cppm` are preserved for reference but excluded from build until reworked.

## Verification in this environment

Python unit tests (5/5) passed. The C++ module was precompiled, compiled, linked, and the example loaded using Clang 17 and `-std=c++2b` as a compatibility smoke check. Clang 23.1.3 / CMake 4.5.0-rc1 are **not installed here**, so the requested toolchain was not tested.

## Standards policy

The source syntax follows HTML Living Standard and CSS Snapshot 2026 **within a documented support profile**. No browser backwards compatibility and no JavaScript. See [docs/standards-profile.md](docs/standards-profile.md). The current implementation is not fully HTML/CSS-conformant; unsupported CSS properties are build-time errors rather than silently ignored.

## v0.4 — contextual compiler diagnostics

`arcuxc` now emits structured errors with stable `AUX-HTML-*`, `AUX-CSS-*`, and `AUX-ASSET-*` codes, source filename, 1-based line/column, source excerpts and underline carets, helpful suggestions for mistyped properties/elements, and ANSI color on interactive terminals (`NO_COLOR` respected). It returns nonzero on failure, does not write a new `.arcuxi` on diagnosed failure, and preserves the `compile_ui(source, root)` programmatic API (diagnostics inherit `ValueError`).

```sh
python tools/arcuxc.py examples/dashboard.html -o dashboard.arcuxi
python tools/arcuxc.py examples/broken.html -o broken.arcuxi --color=always --context-lines=3
python tools/arcuxc.py examples/broken.html -o broken.arcuxi --diagnostic-format=json
python -m unittest discover -s tests -v
```

Diagnostic JSON fields: `severity`, `code`, `message`, `location` (`file`, `line`, `column`, `length`), `hint`, `notes`. Paths/positions are intended for IDE integration. This is a first diagnostics implementation: positions for CSS created by nontrivial HTML normalization, repeated nested CSS blocks, and malformed tokenization require refinement; a full WHATWG tokenizer/parser and lossless CSS source map are not implemented yet. Unsupported CSS remains rejected by design.

## v0.5 — multi-diagnostic collection and contextual source spans

The AOT compiler reports **multiple independent errors** in one invocation. CSS
parsing continues after invalid declarations, selectors and unsupported at-rules;
HTML parsing skips unsupported/forbidden subtrees (including `<script>`) and
continues checking following elements. Value checking continues per CSS property.

* Source positions are mapped to original HTML and `<style>` text using tinycss2
  token coordinates and local offsets (not global substring searches). Repeated
  declarations, separate style blocks, inline styles and Unicode positions are
  covered by tests. Forbidden inline event handler names get attribute-sized spans.
* CLI `--max-errors=N` (default 100) bounds diagnostics. No binary output is
  written if errors are present. Existing output files are **not automatically
  deleted** on compilation failure; consumers should check the exit status.
* `--diagnostic-format=json` now returns a **JSON object with multiple errors**:
  `{ "version": 1, "diagnostics": [...], "summary": { "errors": N } }`.
  Each `location` additionally has `endLine` and `endColumn` (1-based Unicode
  codepoint columns). This is a deliberate breaking change from v0.4's single
  diagnostic JSON object. For LSP clients, convert codepoint columns to UTF-16
  code units; do not pass them through unchanged when astral symbols are present.
* Human diagnostics display each error with surrounding context, source carets,
  suggestion and notes, followed by the total count.

Run all Python tests:

```sh
python -m unittest discover -s tests -v
```

### Remaining scope limitations

The source mapping is **not a full lossless WHATWG HTML/CSS source map**.
HTMLParser normalizes attribute entities; CSS `style=` values with character
references may have column drift; comment stripping and certain CSS escaped
identifiers may change positions in edge cases. Unsupported HTML constructs do
not undergo browser-standard HTML error recovery; malformed nested markup may
still stop semantic analysis. External linked CSS files are not supported in the
compiler. These limitations are separate from the two v0.4 roadmap items:
normal source-level diagnostics and multi-error reporting are implemented, but
full standards-level source fidelity is not yet claimed.

## v0.6 — Flex/Grid AOT layout profile

- Binary image version **2**, style record expanded 52 → 68 bytes to store four additional layout fields; v1 images are intentionally rejected rather than misinterpreted.
- CSS: `justify-content: flex-start | start | center | flex-end | end | space-between`, `align-items: stretch | flex-start | start | center | flex-end | end`, `flex-wrap: nowrap | wrap`, and `grid-template-columns: 1fr 1fr ... | repeat(N, 1fr)` (N ≤ 4096).
- C++ renderer: explicit equal-fraction Grid columns, wrapping flex lines, simple main/cross-axis alignment, early rejection of malformed layout styles.
- CLI native runtime `arcadeui <file.arcuxi> --geometry` exposes node geometry for deterministic integration tests.
- `display:none` descendants are excluded from draw commands.

**Not standards-complete:** Flexbox and Grid still lack `flex-basis`, min/max constraints, `min-content/max-content`, auto-margin behavior, baseline alignment, percentage resolution, Grid spanning, named lines, mixed track types, implicit/explicit track sizing algorithms, and correct general-purpose sizing. Text uses an approximate width. No incremental dirty-graph engine yet. This iteration is a tested, explicitly limited layout implementation, not CSS conformance.

Build with Clang 23 + CMake 4.5 as above. Clang 17 `-std=c++2b` smoke verification is also possible by direct compiler invocation.



## v0.7 — Incremental dirty-layout engine

This release adds an **incremental layout implementation** to `arcade.ui`:

- Dense PMR arrays: a dirty-byte per node, cached intrinsic width/height,
  and a 2-bit measurement-valid mask. No per-node heap objects.
- `Document::invalidate(node_id)` marks the node and its ancestors dirty and
  invalidates their intrinsic size caches. Siblings are remeasured from cache,
  and only geometrically changed or locally dirty branches are arranged.
- `Document::set_size(node_id, width, height)` changes explicit dimensions
  (`-1` means `auto`). `set_background(node_id, rgba)` is paint-only.
- Unchanged `resize(width,height)` calls reuse the prior geometry and draw
  commands instead of re-running layout or paint command creation.
- `Document::layout_stats()` exposes `layout_visits`, `layout_skips`,
  `measure_computations`, `measure_cache_hits`, and `painted_commands`.
- Native `tests/incremental.cpp` verifies no work for unchanged viewport,
  partial recomputation for changed leaf, geometry equality with a fresh full
  layout, and no layout for paint-only updates. Registered as
  `runtime_incremental` in CTest.

**Important limits:** Layout remains the explicit *limited CSS profile* from
v0.6, not conforming Flex/Grid. The current dependency model encodes structural
parent-to-child layout and ancestor intrinsic-size dependencies. In particular,
CSS custom properties, container queries, viewport-relative sizes, advanced
Grid spans, and dynamic DOM mutation are **not** tracked yet. Render command
packing still rebuilds the command array after any geometry change; GPU dirty
ranges and resource reuse are future work. `set_size` updates the style record
selected by the node; the current AOT writer emits one style record per node.

Tested locally using Clang 17 (`-std=c++2b`) and 33 Python tests. The CMake
configuration targets Clang 23 / CMake 4.5 / Ninja and is not verified with
those exact versions in this environment.

## v0.8 — explicit Grid tracks and initial Flex sizing

This is a **measured extension of the CSS profile, not a full normative Flexbox/Grid implementation**.

- Binary `.arcuxi` **v3** (v2 intentionally rejected). Each style is now 92 bytes; a standalone 8-byte-per-entry track table is stored after the styles. Header's first reserved u32 stores the track section size. The SHA-256 payload digest is produced by the AOT compiler but **not yet verified by the native loader**.
- `grid-template-columns`: fixed `<length>px`, fractional `<number>fr`, `minmax(<length>px, 1fr)`, and `repeat(integer, single-track)`; mixed track lists supported. Track sizes are resolved with iterative freezing of fractional tracks whose minimum size exceeds their fractional allocation. Limited implicit row placement and `grid-column: span N` are available.
- `flex-basis: auto | <length>px`, `min-width` and `max-width` in fixed px or auto are compiled as explicit sizing fields. The runtime uses the basis in its existing grow/shrink algorithm and clamps used widths. This is **not** the full Flexbox flexible-length resolution algorithm (iterative min/max freezing is still missing).
- Unsupported grammar is rejected by the existing source-context diagnostics rather than silently approximated.
- Python tests: `python -m unittest discover -s tests -q`. Native geometry test: `runtime_layout_v3`, in addition to `runtime_incremental` and `runtime_smoke`.

### Important limitations

Grid still lacks full intrinsic sizing, min/max-content, multi-axis spanning contributions, dense placement, named lines and explicit start/end placement, row track sizing and auto/min-content semantics. Flexbox still lacks the complete normative min/max loop, percentage resolution, automatic minimum size, baseline alignment and writing modes. This build cannot claim CSS Snapshot 2026 conformance. It accepts an explicitly documented subset of standardized CSS syntax. Layout calculations assume finite sizes; raster quality is not addressed yet.

### v3 wire layout

`header(72) | node[count] (32 bytes each) | style[count] (92 bytes each) | tracks (8 bytes each: u32 kind + f32 value) | utf8 strings | resource records`

Header slots (little endian): magic, version=3, count, node_bytes, style_bytes, string_bytes, resource_bytes, **track_bytes**, reserved, SHA-256. Track kinds: `0=fixed px`, `1=fr weight`, `2=minmax(px,1fr)`; each style contains the track offset and count, plus `grid-column` span.

### Toolchain verification

Clang 17 with `-std=c++2b` compiled and linked the C++ module and native integration tests; project was not run with Clang 23.1.3/CMake 4.5.0-rc1 in the available environment. The intended build remains `cmake --preset debug && cmake --build --preset debug && ctest --test-dir build/debug --output-on-failure`.

## v0.9 — iterative Flex min/max freezing

The runtime now resolves **a supported subset** of flexible main sizes with
iterative freezing, rather than distributing grow/shrink once and clamping the
result. For each flex line it (1) computes base sizes and free space, (2)
distributes positive free space using `flex-grow` or negative free space using
scaled `flex-shrink` weights, (3) clamps to explicit `min-width`/`max-width`
in row layout, (4) freezes violators and redistributes among remaining items.
`justify-content` uses any remaining positive free space after the resolution.

Two native regression cases in `tests/flex_freeze.cpp` verify:

- **Growth:** `300px` row, two `100px` bases, equal grow factors,
  first element `max-width:110px` → widths `110px` and `190px`.
- **Shrinkage:** `200px` row, two `200px` bases, equal shrink factors,
  first element `min-width:160px` → widths `160px` and `40px`.

The project still has **approximate intrinsic measurement** (character-count
heuristic, no font shaping) and is **not yet a CSS-conformant Flexbox engine**.
The automatic minimum-size behavior, percent resolution, cross-axis min/max,
line-breaking nuances, min-content/max-content, multi-pass intrinsic layout,
and several edge cases are not implemented. This work intentionally does not
claim full CSS Snapshot 2026 compliance. The binary format stays `.arcuxi` v3.

Validation in this environment: 39/39 Python tests, native `layout_v3`,
`incremental`, and `flex_freeze` tests passed under Clang 17 `-std=c++2b`.
The intended Clang 23.1.3/CMake 4.5 toolchain was not available for testing.

## v1.0 — Text intrinsic measurement foundation

The previous runtime treated `node.text_size` (UTF-8 **byte** count) as the
number of visible characters, which inflated widths for non-ASCII text.

This version introduces `TextIntrinsic { min_content, max_content, line_height }`
and `Document::intrinsic_text(node_id)` with a **per-node measurement cache**.
For leaf nodes, `measure()` now uses the cached max-content width and line height.

`Document::set_text_measurer(TextMeasure callback, void* context)` allows a
renderer/host to supply accurate shaped-run metrics. Calling it invalidates
both the intrinsic-text cache and dependent layout measurements. Function
signature: `TextIntrinsic (*)(void*, std::string_view, float) noexcept`.
The supplied context and callback must remain alive while in use; results must
be finite and satisfy `0 <= min_content <= max_content` and `line_height >= 0`.
The callback must not throw because it is declared `noexcept`.

The built-in fallback handles UTF-8 scalar decoding (not byte lengths), simple
ASCII whitespace breaks, combining-mark zero advances, and rough full-width
CJK/emoji approximations. **It is NOT font shaping or normative CSS min/max
content**: missing features include font faces, kerning, ligatures, script
shaping, grapheme and UAX #14 line-breaking, hyphenation, bidi, soft wrapping,
vertical writing, line-height semantics, and actual min-content integration into
Grid/Flex automatic minimum-size calculations. Treat it as a deterministic
fallback intended for early-stage testing. HTML/CSS binary format stays v3.

The tests now cover the module's callback/cache invalidation behavior, Unicode
width handling and basic break opportunities; previous Flex/Grid/incremental
regression cases remain passing on the locally available Clang 17 (`-std=c++2b`).

Run `python -m unittest discover -s tests -q` and standard CMake/CTest commands
for the native tests. Clang 23.1.3 and CMake 4.5.0-rc1 were not available here.

## v1.1 — Width-constrained text measurement API (experimental)

- `TextLineLayout`, `TextLineMeasure` and `Document::layout_text(id, width)`
  let the host measure wrapped text with a real font backend. Results are
  validated (finite, nonnegative, positive line count, width within constraint).
- `Document::set_line_measurer` installs a callback. This prototype shares the
  callback context with `set_text_measurer`: install the intrinsic-measurement
  callback first, then install the line-measurement callback using the same
  context. Changing this interface to an owned backend object is recommended.
- Flex row text leaves now use their estimated `min-content` as an automatic
  minimum main size unless `min-width` was explicitly set.
- The built-in line-count fallback is deliberately approximate and is **not**
  Unicode UAX#14 or OpenType shaping. It does not yet feed text height back into
  the Flex/Grid arrange pass, and no full CSS intrinsic layout is claimed.
- Added native `runtime_text_lines` integration test.

## v1.2 — width-constrained text contributions in native layout

This release connects the width-aware text measurement callback to **actual layout
geometry**, rather than exposing it only as a standalone API:

- For a text-only Grid item, the solver measures its block-size using its
  final allocated column/span width (and padding). The resulting height
  determines the row height and following row positions.
- For a text-only row-direction Flex item, cross-size measurement uses the
  main size **after** grow/shrink and min/max resolution; wrapped line heights
  are recomputed from the resulting widths.
- `TextMeasure` and `TextLineMeasure` now have **independent context pointers**.
  Swapping the line-breaker doesn't overwrite the intrinsic metric backend's
  context. This fixes a lifetime/incorrect-dispatch defect in v1.1.
- The width-aware backend is checked for finite, nonnegative results, positive
  height on nonempty text, nonzero line count and width within the constraint.
- Added `runtime_text_geometry` regression (Grid geometry and callback contexts).

**Still not supported:** actual HarfBuzz/FreeType/DirectWrite shaping, UAX #14
line breaking, CSS whitespace modes and full intrinsic Grid track sizing,
fragmentation, baseline alignment, rich inline formatting or glyph rasterization.
The fallback uses heuristic glyph widths and estimates line counts; it does not
promise typographic accuracy. Backend callbacks are `noexcept` function pointers
and are owned by the caller. This implementation is a useful integration stage,
not a spec-conformant CSS text engine.

Run `python -m unittest discover -s tests -q` and build via the presets above.
The C++ changes were additionally smoke-tested on Clang 17 / `-std=c++2b`.
Clang 23.1.3 / CMake 4.5.0-rc1 were not available in the verification environment.

## v1.3 — optional OpenType backend

Added `src/arcade.ui.font.cppm`, a separate native C++ module with private
FreeType and HarfBuzz headers. `arcade.ui` remains buildable without either
library. When installed, CMake detects the packages and builds the optional
`arcade_ui_font` target. Public module usage:

```cpp
import arcade.ui;
import arcade.ui.font;

// Font object MUST outlive the document attachment.
auto document = arcade::ui::Document::load("dashboard.arcuxi");
arcade::ui::font::OpenType font("path/to/font.ttf");
font.attach(document);
document.resize(1280, 720);
font.detach(document);
```

`OpenType` computes true OpenType glyph advances using HarfBuzz and FreeType,
and uses the font's line metrics. This version deliberately uses a **limited
ASCII-whitespace greedy line breaker**. It does NOT implement UAX #14, bidi
paragraph resolution, fallback font runs, variable font CSS axes, glyph
rasterization, or CSS baseline alignment. For unbreakable words it clamps the
reported line width to the available layout width; true overflow geometry will
require changes to the existing `TextLineLayout` contract.

Important: attaching the font stores a borrowed pointer in `Document`.
The font object must not be moved or destroyed while attached. Both callbacks
are single-threaded (FreeType face size changes between measurements).

Build with `-DARCADE_UI_ENABLE_FONT_BACKEND=ON` (default); missing packages
simply disable that backend. Set `-DARCADE_UI_ENABLE_FONT_BACKEND=OFF`
for a dependency-free runtime. The optional backend is **not compiled or
runtime-tested in the current environment**, because HarfBuzz/FreeType
headers are unavailable. Existing 41 Python tests pass.
