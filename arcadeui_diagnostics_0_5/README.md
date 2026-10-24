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
- Deterministic little-endian binary v1, relative string offsets, SHA-256 payload checksum from compiler, packed assets.
- Breadth-first densely-packed topology (contiguous siblings), PMR storage, native module, layout-to-draw batch.
- Callback ID dispatch via `function pointer + context`, hit testing, responsive recomputation via `resize`.
- Python compiler unit tests (10 cases) and runtime smoke test.

## Limitations / next stage

Not a full browser engine: no complex CSS selectors, box sizing variations, `minmax`, intrinsic Grid tracks, CSS percentages, wrapping Flex, accessibility tree, font shaping, animations, or GPU renderer yet. Currently the runtime generates draw commands and can export an SVG preview; no window or GPU rendering. Assets are packed but aren't decoded/rendered. Coroutine input scheduler and dependency-based incremental layout are future work. SHA-256 written by compiler is not yet revalidated at runtime. Format isn't hardened for untrusted images: validate further before loading external content. Text width is approximated. Grid uses heuristic square-column auto placement. `display:none` currently does not completely remove descendants from hit-testing. Intended as foundation, not CSS specification conformance.

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

Native runtime and Flex/Grid are **unchanged** from the previous prototype.
