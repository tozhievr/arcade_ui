#include <cstdio>
#include <exception>
#include <cstdint>
#include <stdexcept>
import arcade.ui.text.runs;
import arcade.ui.unicode;
#include <cassert>
#include <string_view>
using namespace arcade::ui;
int main() {
    try {
        constexpr std::string_view mixed = "Hello 世界 שלום!";
        auto lines = text::segment_visual_lines(mixed, {{0, static_cast<std::uint32_t>(mixed.size())}});
        assert(lines.size() == 1);
        assert(lines[0].runs.size() >= 3);
        bool latin = false, cjk = false, rtl = false;
        for (const auto& run : lines[0].runs) {
            assert(run.begin < run.end && run.end <= mixed.size());
            auto part = mixed.substr(run.begin, run.end - run.begin);
            if (part.find("Hello") != std::string_view::npos)
                latin = true;
            if (part.find("世界") != std::string_view::npos)
                cjk = true;
            if (run.direction == unicode::Direction::rtl)
                rtl = true;
        }
        assert(latin && cjk && rtl);
        constexpr std::string_view two = "abc\nשלום";
        auto separate =
            text::segment_visual_lines(two, {{0, 3}, {4, static_cast<std::uint32_t>(two.size())}});
        assert(separate.size() == 2 && separate[0].runs.size() >= 1 && separate[1].runs.size() >= 1);
        assert(separate[0].runs[0].begin == 0 && separate[0].runs[0].end == 3);
        assert(separate[1].runs[0].begin == 4 && separate[1].runs[0].end == two.size());
        bool rejected = false;
        try {
            (void)text::segment_visual_lines(std::string_view("\xFF", 1), {{0, 1}});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unexpected test failure: %s\\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("Unexpected non-standard test exception\\n", stderr);
        return 1;
    }
}
