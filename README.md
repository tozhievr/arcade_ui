
## v1.4 — Unicode text analysis (ICU)

Added `src/arcade.ui.unicode.cppm`, an independently usable **native C++ module** with an ICU-backed implementation:

- `arcade::ui::unicode::analyze(utf8, paragraph_direction)` returns legal Unicode Line Breaking Algorithm (UAX #14) opportunities as **UTF-8 byte offsets**.
- Returns paragraph-level Unicode Bidirectional Algorithm (UAX #9) visual runs with UTF-8 start/end ranges and LTR/RTL directions.
- Converts ICU's UTF-16 offsets to UTF-8 byte offsets, including supplementary-plane characters (emoji).
- Rejects invalid UTF-8 rather than replacing corrupted input implicitly.
- `ARCADE_UI_ENABLE_UNICODE=ON` enables the optional ICU component; it is built only when ICU development libraries are detected.
- When the optional FreeType/HarfBuzz backend is also present, it uses Unicode break opportunities for line wrapping and intrinsic segment measurements. Without ICU, the legacy ASCII whitespace behavior is retained.

### Test status

- Python compiler suite: **41/41 passed**.
- Standalone Unicode module: **compiled and executed** with Clang 17 and system ICU 76; tests cover Latin whitespace, Chinese text, bidi Hebrew fragments, UTF-8 emoji offsets, invalid UTF-8.
- FreeType/HarfBuzz backend: **not compiled in this environment**, because development headers/pkg-config packages are missing. Its ICU integration is therefore unverified.
- Clang 23.1.3 / CMake 4.5.0-rc1: **not tested** here.

### Remaining standard-compliance work

This is **not full CSS Text**, and paragraph-level bidi runs are **not yet sufficient for CSS line layout**. The next steps are CSS white-space processing, style-aware segmentation, per-line UAX #9 resolution after wrapping, script/language/font fallback runs, contextual shaping across break opportunities, real OpenType glyph runs, and GPU glyph atlas rendering. `OpenType::lines()` currently computes approximate greedy widths from independently shaped break segments. It must not be used as a pixel-exact shaping/layout engine for Arabic/Indic scripts; shaping entire runs and splitting at proper cluster boundaries is required.

The Unicode module exports no third-party ICU types. The binary `.arcuxi` format remains v3.

## v1.5 — Per-line Unicode bidi resolution

Added `unicode::resolve_visual_lines(utf8, line_spans, paragraph_direction)` to `arcade.ui.unicode`.

- Input ranges and returned visual-run positions are UTF-8 byte offsets into the original text.
- Validates line range bounds and code point boundaries (including supplementary-plane characters).
- Uses ICU `ubidi_setPara()` followed by `ubidi_setLine()` for each requested line, instead of incorrectly reusing paragraph-level visual runs.
- Returns visual runs in display order with original byte spans and text direction.
- New `runtime_unicode_lines` test covers mixed Hebrew/Latin lines, emoji boundaries and invalid split offsets.
- API does **not** reorder glyphs, implement CSS `white-space` or `overflow-wrap`, generate shaped glyph runs, perform font fallback or assemble line breaks. The caller supplies line spans after line fitting; this is one verified building block, not complete CSS Text.

### Verified here

- Clang 17 `-std=c++2b` + ICU: unicode module and new native test compile/link/run: **PASS**.
- Python compiler regression suite: **41 tests passed**.
- Clang 23.1.3/CMake 4.5.0-rc1 and HarfBuzz/FreeType-enabled build: not available for verification here.

## v1.6 — CSS Text line-fitting foundation

Added `src/arcade.ui.text.cppm` (`import arcade.ui.text;`), separately from the existing Unicode module. It exposes `text::fit(text, width, style, measure, context)`, a style profile (`WhiteSpace`, `OverflowWrap`) and `Line`/`Fragment` records. Its input is UTF-8 and fragment offsets point back into the original string. Unicode UAX #14 breaks and per-line UAX #9 runs are supplied by ICU via `arcade.ui.unicode`.

Supported initial cases: basic ASCII whitespace collapsing; forced LF/CRLF lines under `pre`, `pre-wrap`, `pre-line` and `break-spaces`; no-wrap vs wrapping; emergency UTF-8 codepoint splitting under `anywhere` / `break-word`; a measurement callback for line width; rejection of invalid UTF-8, invalid widths and callbacks. Public module interfaces contain no ICU headers.

**Important implementation limits:** This is NOT full CSS Text Level 3/4. The line fitter is a proof-of-concept: it does not yet correctly handle all trailing/leading collapsible whitespace at line boundaries, tabs, soft hyphens, CSS segment-break transformation, word-break/hyphens, grapheme-based emergency breaks, spans/styles crossing text runs, hanging punctuation, line-height/baselines, or script-sensitive shaping/measurement. Its 'anywhere' emergency mode currently splits by Unicode codepoint, which can split a grapheme cluster. Returning visual runs does not mean glyph reordering/shaping is implemented. ICU and a caller-provided width callback are required. It is not yet wired into `Document` layout or the binary CSS style schema; the text module is independently testable and ready for incremental integration. For production display use an actual OpenType backend and cluster-aligned fitting.

### Verified

- Clang 17 (`-std=c++2b`), ICU system libraries: `arcade.ui.unicode` and new `arcade.ui.text` module compiled, linked and executed: **PASS**.
- New `tests/text_pipeline.cpp`: basic wrap, preformatted hard newline, emergency wrap, mixed bidi and ASCII whitespace collapse: **PASS**.
- Previous Python suite: **41/41 PASS**.
- CMake 4.5 / Clang 23.1.3 not available for verification here.

## v1.7 — Grapheme-safe line breaking and Document bridge

- `arcade.ui.unicode`: `grapheme_boundaries(utf8)` uses ICU UAX #29 character boundaries and returns byte offsets in original UTF-8; rejects malformed UTF-8.
- `arcade.ui.text`: emergency `overflow-wrap` splits at **extended grapheme cluster** boundaries instead of UTF-8 code-point boundaries. This avoids splitting combining accents, emoji modifiers and ZWJ emoji sequences.
- `arcade.ui.text.document`: non-owning `DocumentAdapter` installs the CSS Text line fitter into the existing `Document::set_line_measurer()` interface. Supply a real font-aware width callback; the adapter has no glyph metrics of its own. The adapter must outlive the attachment, and caller must detach before destruction. Existing `Document` API currently rejects overflowed line widths; the bridge reports such results as an invalid measurement rather than hiding overflow.
- New regression tests: `runtime_text_graphemes` and `runtime_text_document_bridge`.
- **Scope:** not a complete CSS Text implementation. Width measurement remains caller-provided; no shaping, style runs, font fallback, full white-space processing, baseline or GPU glyph atlas in this iteration. `Document` CSS properties are not yet mapped automatically to text::Style — the caller provides the style to `DocumentAdapter`.
- **Verified here:** Clang 17 C++2b manual module builds; ICU 76.1; grapheme test, bridge test against freshly compiled `text_layout_rows.arcuxi`, original text pipeline test; 41/41 Python compiler tests.
- CMake 4.5 and Clang 23.1.3 unavailable here, so the preset/target build has not been verified.
