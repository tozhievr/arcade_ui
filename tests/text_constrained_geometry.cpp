#include <cmath>
#include <cstdio>
#include <exception>
#include <string_view>
import arcade.ui;
using namespace arcade::ui;
struct Counter {
    int calls {};
};
static TextIntrinsic intrinsic(void* ptr, std::string_view, float) noexcept {
    ++static_cast<Counter*>(ptr)->calls;
    return {25.F, 150.F, 20.F};
}
static TextLineLayout lines(void* ptr, std::string_view, float, float width) noexcept {
    ++static_cast<Counter*>(ptr)->calls;
    if (width < 120.F)
        return {width, 40.F, 2};
    return {100.F, 20.F, 1};
}
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    try {
        auto document = Document::load(argv[1]);
        Counter text_calls {}, line_calls {};
        document.set_text_measurer(intrinsic, &text_calls);
        document.set_line_measurer(lines, &line_calls);
        auto metrics = document.intrinsic_text(1);
        if (text_calls.calls != 1 || line_calls.calls != 0 || metrics.min_content != 25.F)
            return 3;
        document.resize(200.F, 200.F);
        auto geometry = document.geometry();
        if (geometry.size() != 4)
            return 4;
        // Two columns of 100px, first row height determined by line wrapping.
        if (std::fabs(geometry[3].y - 40.F) > 0.02F) {
            std::fprintf(stderr, "second row y=%f, expected 40\n", geometry[3].y);
            return 5;
        }
        if (line_calls.calls == 0)
            return 6;
        if (text_calls.calls < 1)
            return 7;
        std::puts("PASS independent callback contexts and constrained Grid row heights");
        return 0;
    } catch (std::exception const& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 20;
    }
}
