
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
