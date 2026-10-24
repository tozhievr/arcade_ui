#include <cassert>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string_view>
import arcade.ui;
import arcade.ui.text.document;
import arcade.ui.text;
using namespace arcade::ui;
static float measure(void*, std::string_view text) noexcept {
    return float(text.size()) * 8.F;
}
int main(int argc, char** argv) {
    try {
        if (argc < 2)
            return 2;
        auto document = Document::load(argv[1]);
        text::DocumentAdapter adapter(
            measure, nullptr, {text::WhiteSpace::normal, text::OverflowWrap::anywhere});
        adapter.attach(document);
        document.resize(200.F, 200.F);
        auto lines = document.layout_text(1, 100.F);
        assert(lines.line_count >= 1 && std::isfinite(lines.height));
        adapter.detach(document);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unexpected test failure: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("Unexpected non-standard test exception\n", stderr);
        return 1;
    }
}
