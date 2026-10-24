# Application, RenderDriver & Mock Tests

Arcade UI uses a batch-oriented renderer interface. Render drivers do not parse
HTML/CSS or access the layout tree; the compiler and `Document` prepare
`std::span<const DrawCommand>` ahead of submission.

## Modules

- `import arcade.ui.contracts;` — `FrameInfo`, `RenderDriver<D>` and `UiApplication<A,D>`.
- `import arcade.ui.app;` — `AppBuilder<D>`, `Application<D>`, `AppError`.
- `import arcade.ui.mock;` — test backend `MockRenderDriver` and `RecordedDrawCommand`.

```cpp
import arcade.ui.app;
import arcade.ui.mock;

using namespace arcade::ui;

auto builder = AppBuilder<MockRenderDriver>{};
builder.source("dashboard.arcuxi")
       .renderer(MockRenderDriver{})
       .viewport(1280.0f, 720.0f);

auto built = std::move(builder).build();
if (!built) {
    // built.error().code / built.error().message
    return;
}
auto& app = *built;
if (!app.frame()) {
    // Renderer declined begin/submit/end, or layout failed.
}
auto captured = app.render_driver().commands();
```

`Application` owns the loaded `Document` and its render driver. Frame rendering
is currently **synchronous**, with the order:

1. `document.resize(width,height)`;
2. `begin_frame(FrameInfo)`;
3. `submit(document.commands())`;
4. `end_frame()` (also called when `submit` rejects the batch).

`AppBuilder` returns `std::expected<Application<D>,AppError>`, with
distinct errors for a missing source, missing renderer, invalid dimensions, or
failure to load `.arcuxi`.

## Mock guarantees

`MockRenderDriver` deep-copies text from `DrawCommand::text` to prevent
dangling source views in test assertions. It records frame dimensions, counts
of begin/submit/end calls, and painted commands. Injected failures make
frame lifecycle behaviour testable without a graphics API.

The mock makes **no claim about raster quality or GPU output**. It verifies
draw-command geometry, ordering, colors, text and submission semantics.
Rendering snapshots and pixel tests require a real backend later.

### Validation

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --test-dir build/debug -R runtime_app_contracts --output-on-failure
```

The new C++ Modules integration test is in `tests/app_contracts.cppm` and
`tests/app_contracts_main.cpp`. The test should run without ICU or
HarfBuzz/FreeType installed, but needs a compatible Clang/CMake modules toolchain.
