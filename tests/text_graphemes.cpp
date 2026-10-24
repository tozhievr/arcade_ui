#include <cassert>
#include <string_view>
#include <algorithm>
#include <vector>
#include <cstdint>
import arcade.ui.text;
import arcade.ui.unicode;
using namespace arcade::ui;
static float width(void*,std::string_view s) noexcept { return static_cast<float>(s.size()); }
int main() {
    // a + combining acute, emoji with skin-tone modifier, and a ZWJ sequence.
    constexpr std::string_view source="a\xcc\x81\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb";
    auto boundaries=unicode::grapheme_boundaries(source);
    assert((boundaries==std::vector<std::uint32_t>{0,3,11,22}));
    auto lines=text::fit(source,1.f,{text::WhiteSpace::normal,text::OverflowWrap::anywhere},width,nullptr);
    for(auto const& line:lines.lines)for(auto const& fragment:line.fragments){
        assert(std::ranges::find(boundaries,fragment.begin)!=boundaries.end());
        assert(std::ranges::find(boundaries,fragment.end)!=boundaries.end());
    }
    assert(lines.lines.size()>=3);
}
