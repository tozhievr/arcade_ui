#include <algorithm>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
import arcade.ui.unicode;
using namespace arcade::ui::unicode;
static void require(bool value, const char* reason) {
    if (!value)
        throw std::runtime_error(reason);
}
int main() {
    try {
        {
            auto a = analyze("hello world");
            require(a.breaks.front() == 0 && a.breaks.back() == 11, "ASCII endpoints");
            require(std::find(a.breaks.begin(), a.breaks.end(), 6) != a.breaks.end(),
                    "ASCII space break");
            require(a.visual_runs.size() == 1 && a.visual_runs[0].direction == Direction::ltr,
                    "LTR run");
        }
        {
            const std::string text = "你好世界";
            auto a = analyze(text);
            require(a.breaks.front() == 0 && a.breaks.back() == text.size(), "CJK endpoints");
            require(std::find(a.breaks.begin(), a.breaks.end(), 3) != a.breaks.end(),
                    "CJK line break");
            for (auto b : a.breaks)
                require(b % 3 == 0, "UTF8 boundary");
        }
        {
            const std::string text = "abc שלום def";
            auto a = analyze(text);
            bool rtl = false;
            for (auto r : a.visual_runs) {
                require(r.byte_end <= text.size() && r.byte_end > r.byte_begin, "run offsets");
                rtl |= r.direction == Direction::rtl;
            }
            require(rtl, "RTL segment");
        }
        {
            const std::string text = "A😀 B";
            auto a = analyze(text);
            require(a.breaks.back() == text.size(), "supplementary-plane mapping");
            for (auto b : a.breaks)
                require(b != 2 && b != 3 && b != 4, "no mid-emoji boundary");
        }
        try {
            (void)analyze(std::string("\xC3\x28", 2));
            return 2;
        } catch (const std::invalid_argument&) {
            std::puts("Expected invalid Unicode input rejected");
        }
        std::puts("unicode analysis PASS");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unexpected test failure: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("Unexpected non-standard test exception\n", stderr);
        return 1;
    }
}
