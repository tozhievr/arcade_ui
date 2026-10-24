module;
#include <cstdint>
#include <vector>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <utility>
export module arcade.ui.mock;
export import arcade.ui.contracts;
import arcade.ui;

export namespace arcade::ui {
    struct RecordedDrawCommand {
        Rect rect{};
        std::uint32_t rgba{};
        std::pmr::string text;
        std::uint32_t text_rgba{};
        float font_size{};
        explicit RecordedDrawCommand(std::pmr::memory_resource* resource)
            : text(resource) {}
    };

    // Test-only backend, performs no rendering. Text strings are deep-copied
    // because DrawCommand::text is a non-owning string_view.
    class MockRenderDriver {
        std::pmr::memory_resource* resource_;
        std::pmr::vector<RecordedDrawCommand> commands_;
        FrameInfo last_frame_{};
        std::uint64_t starts_{}, submits_{}, finishes_{};
        bool active_{};
        bool fail_begin_{}, fail_submit_{}, fail_end_{};

    public:
        explicit MockRenderDriver(
            std::pmr::memory_resource* resource = std::pmr::get_default_resource())
            : resource_(resource ? resource : std::pmr::get_default_resource()),
              commands_(resource_) {}

        MockRenderDriver(const MockRenderDriver&) = delete;
        MockRenderDriver& operator=(const MockRenderDriver&) = delete;
        MockRenderDriver(MockRenderDriver&&) = default;
        MockRenderDriver& operator=(MockRenderDriver&&) = default;

        [[nodiscard]] bool begin_frame(FrameInfo frame) noexcept {
            if (active_ || fail_begin_) return false;
            commands_.clear();
            last_frame_ = frame;
            active_ = true;
            ++starts_;
            return true;
        }
        [[nodiscard]] bool submit(std::span<const DrawCommand> batch) noexcept {
            if (!active_ || fail_submit_) return false;
            try {
                for (const auto& source : batch) {
                    RecordedDrawCommand& target = commands_.emplace_back(resource_);
                    target.rect = source.rect;
                    target.rgba = source.rgba;
                    target.text.assign(source.text.begin(), source.text.end());
                    target.text_rgba = source.text_rgba;
                    target.font_size = source.font_size;
                }
                ++submits_;
                return true;
            } catch (...) {
                commands_.clear();
                return false;
            }
        }
        [[nodiscard]] bool end_frame() noexcept {
            if (!active_) return false;
            active_ = false;
            ++finishes_;
            return !fail_end_;
        }
        void inject_failures(bool begin, bool submit, bool end) noexcept {
            fail_begin_ = begin; fail_submit_ = submit; fail_end_ = end;
        }
        [[nodiscard]] std::span<const RecordedDrawCommand> commands() const noexcept {
            return commands_;
        }
        [[nodiscard]] FrameInfo last_frame() const noexcept { return last_frame_; }
        [[nodiscard]] std::uint64_t started_frames() const noexcept { return starts_; }
        [[nodiscard]] std::uint64_t submitted_batches() const noexcept { return submits_; }
        [[nodiscard]] std::uint64_t ended_frames() const noexcept { return finishes_; }
    };
    static_assert(RenderDriver<MockRenderDriver>);
}
