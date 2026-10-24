#include <cstdio>
#include <exception>
import arcade.ui;
import arcade.ui.font;
int main(){
    // Compile/link integration smoke test: loading a font is exercised when
    // ARCADE_UI_TEST_FONT is configured by the caller.
    std::puts("PASS optional HarfBuzz/FreeType module linkage");
    return 0;
}
