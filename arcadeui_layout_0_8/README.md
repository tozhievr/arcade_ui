# Arcade UI — AOT prototype 0.3

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
