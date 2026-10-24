#include <cassert>
#include <string_view>
#include <cmath>
import arcade.ui;
import arcade.ui.text.document;
import arcade.ui.text;
using namespace arcade::ui;
static float measure(void*,std::string_view text) noexcept { return float(text.size())*8.f; }
int main(int argc,char** argv) {
    if(argc<2)return 2;
    auto document=Document::load(argv[1]);
    text::DocumentAdapter adapter(measure,nullptr,{text::WhiteSpace::normal,text::OverflowWrap::anywhere});
    adapter.attach(document);
    document.resize(200.f,200.f);
    auto lines=document.layout_text(1,100.f);
    assert(lines.line_count>=1 && std::isfinite(lines.height));
    adapter.detach(document);
}
