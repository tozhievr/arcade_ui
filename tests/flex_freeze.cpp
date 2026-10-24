#include <cmath>
#include <cstdio>
#include <exception>
import arcade.ui;
int main(int argc, char** argv) {
    if (argc != 3)
        return 2;
    try {
        auto grow = arcade::ui::Document::load(argv[1]);
        grow.resize(300, 50);
        auto g = grow.geometry();
        if (g.size() != 3 || std::fabs(g[1].width - 110.F) > 0.02F ||
            std::fabs(g[2].width - 190.F) > 0.02F) {
            std::fprintf(stderr, "grow freeze: %.3f %.3f\n", g[1].width, g[2].width);
            return 3;
        }
        auto shrink = arcade::ui::Document::load(argv[2]);
        shrink.resize(200, 50);
        auto s = shrink.geometry();
        if (s.size() != 3 || std::fabs(s[1].width - 160.F) > 0.02F ||
            std::fabs(s[2].width - 40.F) > 0.02F) {
            std::fprintf(stderr, "shrink freeze: %.3f %.3f\n", s[1].width, s[2].width);
            return 4;
        }
        std::puts("PASS flex grow/shrink iterative min/max freezing");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 20;
    }
}
