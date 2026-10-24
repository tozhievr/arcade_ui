import arcade.ui.text;
#include <cassert>
#include <cmath>
#include <string_view>
using namespace arcade::ui::text;
static float measure(void*, std::string_view s) noexcept {
    return float(s.size() * 10);
}
int main() {
    auto a = fit("one two three", 70, {}, measure, nullptr);
    assert(a.lines.size() >= 2);
    auto b = fit("a\nb", 100, {WhiteSpace::pre, OverflowWrap::normal}, measure, nullptr);
    assert(b.lines.size() == 2 && b.lines[0].forced);
    auto c = fit("abcdef", 20, {WhiteSpace::normal, OverflowWrap::anywhere}, measure, nullptr);
    assert(c.lines.size() >= 3);
    auto d = fit("hello שלום world", 70, {}, measure, nullptr);
    assert(!d.lines.empty() && !d.lines[0].visual_runs.empty());
    auto e = fit("a  b", 100, {}, measure, nullptr);
    assert(e.lines[0].width == 30);
}
