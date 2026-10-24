module;
#include <cmath>
#include <cstdint>
#include <exception>
#include <expected>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
export module arcade.ui.app;
export import arcade.ui.contracts;

export namespace arcade::ui {
    enum class AppErrorCode : std::uint8_t {
        missing_source,
        missing_driver,
        invalid_viewport,
        load_failure
    };

    struct AppError {
        AppErrorCode code{};
        std::string message;
    };

    // Owns Document and renderer. A frame is a synchronous transaction over
    // a snapshot of draw commands. Async scheduling belongs to a later layer.
    template<RenderDriver D>
    class Application {
        Document document_;
        D driver_;
        float width_{};
        float height_{};
        std::uint64_t completed_frames_{};

    public:
        Application(Document document, D driver, float width, float height)
            : document_(std::move(document)),
              driver_(std::move(driver)), width_(width), height_(height) {}

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = default;
        Application& operator=(Application&&) = default;

        [[nodiscard]] bool frame() {
            // Document::resize may fail if layout metrics are invalid.
            try {
                document_.resize(width_, height_);
            } catch (const std::exception&) {
                return false;
            }
            if (!driver_.begin_frame({width_, height_, completed_frames_}))
                return false;
            const bool submitted = driver_.submit(document_.commands());
            // Always close a started frame, even if submit failed.
            const bool ended = driver_.end_frame();
            if (submitted && ended) ++completed_frames_;
            return submitted && ended;
        }

        [[nodiscard]] bool resize(float width, float height) noexcept {
            if (!std::isfinite(width) || !std::isfinite(height) ||
                width < 0 || height < 0) return false;
            width_ = width;
            height_ = height;
            return true;
        }

        [[nodiscard]] Document& document() noexcept { return document_; }
        [[nodiscard]] const Document& document() const noexcept { return document_; }
        [[nodiscard]] D& render_driver() noexcept { return driver_; }
        [[nodiscard]] const D& render_driver() const noexcept { return driver_; }
        [[nodiscard]] std::uint64_t completed_frames() const noexcept {
            return completed_frames_;
        }
    };

    template<RenderDriver D>
    class AppBuilder {
        std::string source_;
        std::optional<D> renderer_;
        std::pmr::memory_resource* resource_ = std::pmr::get_default_resource();
        float width_ = 960.f;
        float height_ = 540.f;

    public:
        AppBuilder() = default;
        AppBuilder(const AppBuilder&) = delete;
        AppBuilder& operator=(const AppBuilder&) = delete;
        AppBuilder(AppBuilder&&) = default;
        AppBuilder& operator=(AppBuilder&&) = default;

        AppBuilder& source(std::string path) {
            source_ = std::move(path);
            return *this;
        }

        AppBuilder& renderer(D driver) {
            renderer_.emplace(std::move(driver));
            return *this;
        }

        AppBuilder& viewport(float width, float height) noexcept {
            width_ = width;
            height_ = height;
            return *this;
        }

        AppBuilder& memory_resource(std::pmr::memory_resource* resource) noexcept {
            resource_ = resource ? resource : std::pmr::get_default_resource();
            return *this;
        }

        [[nodiscard]] std::expected<Application<D>, AppError> build() && {
            if (source_.empty()) {
                return std::unexpected(AppError{AppErrorCode::missing_source,
                                               "An ArcUXI source path is required"});
            }
            if (!renderer_) {
                return std::unexpected(AppError{AppErrorCode::missing_driver,
                                               "A render driver is required"});
            }
            if (!std::isfinite(width_) || !std::isfinite(height_) ||
                width_ < 0 || height_ < 0) {
                return std::unexpected(AppError{AppErrorCode::invalid_viewport,
                                               "Viewport must have finite nonnegative dimensions"});
            }
            try {
                Document document = Document::load(source_, resource_);
                return std::expected<Application<D>, AppError>(
                    std::in_place, std::move(document),
                    std::move(*renderer_), width_, height_);
            } catch (const std::exception& e) {
                return std::unexpected(AppError{AppErrorCode::load_failure,e.what()});
            }
        }
    };
}
