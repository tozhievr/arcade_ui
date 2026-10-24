module;
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
export module arcade.ui.test.contracts;
// This is a named module test to ensure exported types work for importers.
import arcade.ui.app;
import arcade.ui.mock;

static_assert(arcade::ui::RenderDriver<arcade::ui::MockRenderDriver>);
static_assert(arcade::ui::UiApplication<arcade::ui::Application<arcade::ui::MockRenderDriver>,
                                        arcade::ui::MockRenderDriver>);

export int exercise_app(std::string path) {
    using namespace arcade::ui;
    auto builder = AppBuilder<MockRenderDriver> {};
    builder.source(std::move(path)).renderer(MockRenderDriver {}).viewport(960.F, 540.F);
    auto app = std::move(builder).build();
    if (!app) {
        std::fprintf(stderr, "AppBuilder error: %s\n", app.error().message.c_str());
        return 1;
    }
    if (!app->frame())
        return 2;
    auto source = app->document().commands();
    auto captured = app->render_driver().commands();
    if (source.size() != captured.size())
        return 3;
    bool nonempty = false;
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i].rect.x != captured[i].rect.x || source[i].rect.y != captured[i].rect.y ||
            source[i].rect.width != captured[i].rect.width || source[i].rgba != captured[i].rgba ||
            source[i].text != std::string_view(captured[i].text))
            return 4;
        nonempty = nonempty || !captured[i].text.empty();
    }
    if (!nonempty)
        return 5;
    if (app->render_driver().started_frames() != 1 ||
        app->render_driver().submitted_batches() != 1 || app->render_driver().ended_frames() != 1)
        return 6;
    if (!app->frame() || app->completed_frames() != 2)
        return 7;
    if (!app->resize(1280.F, 720.F) || !app->frame())
        return 8;
    if (app->render_driver().last_frame().width != 1280.F)
        return 9;
    if (app->resize(-1.F, 100.F))
        return 10;
    app->render_driver().inject_failures(false, true, false);
    if (app->frame())
        return 11;
    if (app->render_driver().started_frames() != 4 || app->render_driver().ended_frames() != 4)
        return 12;

    AppBuilder<MockRenderDriver> bad;
    auto missing = std::move(bad).build();
    if (missing || missing.error().code != AppErrorCode::missing_source)
        return 13;
    std::puts("PASS: mock render driver and AppBuilder");
    return 0;
}
