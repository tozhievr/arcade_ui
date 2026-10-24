module;
#include <concepts>
#include <cstdint>
#include <span>
export module arcade.ui.contracts;
import arcade.ui;

export namespace arcade::ui {
    struct FrameInfo {
        float width{};
        float height{};
        std::uint64_t index{};
    };

    // The backend receives immutable, already-laid-out paint commands.
    // No DOM, CSS or HTML parsing is part of this contract.
    template<class D>
    concept RenderDriver = std::movable<D> &&
        requires(D& driver, FrameInfo frame, std::span<const DrawCommand> batch) {
            { driver.begin_frame(frame) } noexcept -> std::same_as<bool>;
            { driver.submit(batch) } noexcept -> std::same_as<bool>;
            { driver.end_frame() } noexcept -> std::same_as<bool>;
        };

    template<class A, class D>
    concept UiApplication = RenderDriver<D> &&
        requires(A& app, float w, float h) {
            { app.frame() } -> std::same_as<bool>;
            { app.resize(w, h) } -> std::same_as<bool>;
            { app.document() } -> std::same_as<Document&>;
            { app.render_driver() } -> std::same_as<D&>;
        };
}
