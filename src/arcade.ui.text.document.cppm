module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
export module arcade.ui.text.document;
import arcade.ui;
import arcade.ui.text;

export namespace arcade::ui::text {
    // Non-owning bridge. Must remain alive while attached to a Document.
    // Width measurement is deliberately supplied by a shaping backend.
    // No browser font or fake glyph advance is hidden in this adapter.
    class DocumentAdapter {
        Measure measure_ {};
        void* context_ {};
        Style style_ {};
        float line_height_multiplier_ {1.2F};
        static TextLineLayout
        callback(void* state, std::string_view text, float font_size, float width) noexcept {
            auto& self = *static_cast<DocumentAdapter*>(state);
            try {
                auto result = fit(text, width, self.style_, self.measure_, self.context_);
                float measured = 0.F;
                for (const auto& line : result.lines)
                    measured = std::max(measured, line.width);
                // Document's current callback contract forbids overflow widths.
                // Return an invalid measurement instead of silently clipping.
                if (measured > width + 0.01F)
                    return {-1.F, -1.F, 0};
                return {measured,
                        static_cast<float>(result.lines.size()) * font_size *
                            self.line_height_multiplier_,
                        static_cast<std::uint32_t>(result.lines.size())};
            } catch (...) {
                return {-1.F, -1.F, 0};
            }
        }

    public:
        DocumentAdapter(Measure measure,
                        void* context = nullptr,
                        Style style = {},
                        float line_height = 1.2F)
            : measure_(measure), context_(context), style_(style),
              line_height_multiplier_(line_height) {}
        void attach(Document& document) { document.set_line_measurer(&callback, this); }
        void detach(Document& document) { document.set_line_measurer(nullptr, nullptr); }
    };
} // namespace arcade::ui::text
