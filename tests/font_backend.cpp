#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>
import arcade.ui.font;

int main(int argc, char** argv) {
    // Provide a real TTF/OTF path explicitly. Without one this optional test skips.
    const char* path = argc > 1 ? argv[1] : std::getenv("ARCADE_UI_TEST_FONT");
    if (!path || !*path) {
        std::puts("SKIP: set ARCADE_UI_TEST_FONT to a readable TTF/OTF font");
        return 77;
    }
    try {
        arcade::ui::font::OpenType font(path);
        auto initial = font.cache_stats();
        auto first = font.shape("Hello Arcade", 16.F);
        assert(first && !first->ids.empty());
        assert(first->ids.size() == first->clusters.size());
        assert(first->advance_x.size() == first->ids.size());
        assert(std::isfinite(first->total_advance));
        auto after_first = font.cache_stats();
        assert(after_first.misses > initial.misses);

        // Second shaping at the same size must not rerun HarfBuzz.
        auto second = font.shape("Hello Arcade", 16.F);
        assert(second && !second->ids.empty());
        assert(font.cache_stats().hits > after_first.hits);

        // Font size is part of cache key.
        auto different_size = font.shape("Hello Arcade", 22.F);
        assert(different_size && !different_size->ids.empty());
        auto after_size = font.cache_stats();
        assert(after_size.misses > after_first.misses);

        // Intrinsic measurement uses the same glyph cache.
        auto metrics = font.intrinsic("Hello Arcade", 16.F);
        assert(metrics.max_content > 0 && metrics.line_height > 0);
        assert(font.cache_stats().hits > after_size.hits);

        auto unicode = font.shape("Привет 世界", 16.F);
        assert(unicode);
        for (auto cluster : unicode->clusters)
            assert(cluster < std::string_view("Привет 世界").size());
        font.clear_cache();
        assert(font.cache_stats().entries == 0);
        assert(font.cache_stats().glyphs == 0);
        std::puts("PASS: real OpenType shaping and SoA cache");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
