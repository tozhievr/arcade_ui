# OpenType Shaping Cache (v2.0 development)

`arcade.ui.font` now imports `arcade.ui.text.cache` and has an **instance-owned**
`ShapedRunCache`. Normal `OpenType::intrinsic()` and `OpenType::lines()`
measurements share cached glyph runs, so unchanged segments with the same font
pixel size do not need to invoke HarfBuzz again.

```cpp
import arcade.ui.font;

arcade::ui::font::OpenType face("assets/fonts/Inter.ttf");
auto shaped = face.shape("Hello Arcade", 16.0f);
if (shaped) {
    // shaped->ids / clusters / advance_x / offset_x ...
}
auto stats = face.cache_stats();
face.clear_cache();
```

**Ownership:** the returned `GlyphRunView` is a non-owning collection of spans.
It is valid only while the `OpenType` instance lives and until the next
operation that modifies its glyph cache. Do not retain views across calls that
may shape new strings. The instance is **not thread-safe**, as the FT_Face
pixel size and cache are mutable.

**Cache identity:** a cache belongs to exactly one OpenType face, so its private
`face_id` and generation remain constant. The rounded pixel size is included
in its `ShapeKey`. Segment direction, script and language are guessed by
HarfBuzz from the UTF-8 text; explicit shaping settings/features are not yet
part of this API. More advanced shaping must extend the key accordingly.

**Important limitations:** the wrapping algorithm still greedily measures
segments, not entire contextual script runs. HarfBuzz integration here makes
measured segments reusable; it does **not** establish full CSS Text conformance,
font fallback, GPU glyph rendering or correct Arabic/Indic re-shaping at
line breaks.

## Test

`runtime_font_backend` now performs real shaping and cache hit/miss checks if
a font path is supplied via `ARCADE_UI_TEST_FONT`. It skips (code 77) when
no path is supplied. The optional test target is only built when both
HarfBuzz and FreeType are found. Example:

```sh
ARCADE_UI_TEST_FONT=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
  ctest --test-dir build/debug -R runtime_font_backend --output-on-failure
```

The changes were authored without installed HarfBuzz/FreeType headers in the
authoring environment. A successful build and integration test on a machine
with both libraries is still required before treating the backend as verified.
