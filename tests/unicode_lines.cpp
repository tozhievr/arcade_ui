#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
import arcade.ui.unicode;
using namespace arcade::ui::unicode;
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main() {
    const std::string t = "abc שלום def";
    auto out = resolve_visual_lines(t, {{0, 4}, {4, static_cast<std::uint32_t>(t.size())}});
    check(out.size() == 2, "line count");
    check(out[0].size() == 1 && out[0][0].direction == Direction::ltr, "first line LTR");
    bool rtl = false;
    for (auto run : out[1]) {
        rtl |= run.direction == Direction::rtl;
        check(run.byte_begin >= 4 && run.byte_end <= t.size(), "range");
    }
    check(rtl, "second line RTL");
    const std::string emoji = "a😀b";
    auto e = resolve_visual_lines(emoji, {{0, 5}, {5, static_cast<std::uint32_t>(emoji.size())}});
    check(e.size() == 2, "emoji lines");
    try {
        (void)resolve_visual_lines(emoji, {{0, 2}});
        return 2;
    } catch (const std::invalid_argument&) {}
    std::puts("per-line bidi PASS");
}
