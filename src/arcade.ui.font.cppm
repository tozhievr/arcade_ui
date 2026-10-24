module;
#include <ft2build.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>
export module arcade.ui.font;
export import arcade.ui;
export import arcade.ui.text.cache;
#ifdef ARCADE_UI_HAS_UNICODE
import arcade.ui.unicode;
#endif

export namespace arcade::ui::font {
    // Deliberately small, standalone OpenType backend. The implementation owns
    // FT_Face and hb_font_t; callers must keep this object alive while attached
    // to a Document. Not thread-safe (FT_Face pixel size is mutable).
    class OpenType {
        struct State;
        std::unique_ptr<State> state_;
        static TextIntrinsic intrinsic_cb(void*, std::string_view, float) noexcept;
        static TextLineLayout line_cb(void*, std::string_view, float, float) noexcept;

    public:
        explicit OpenType(std::string_view file_path);
        ~OpenType();
        OpenType(const OpenType&) = delete;
        OpenType& operator=(const OpenType&) = delete;
        OpenType(OpenType&&) noexcept;
        OpenType& operator=(OpenType&&) noexcept;
        void attach(Document& document);
        void detach(Document& document) noexcept;
        [[nodiscard]] TextIntrinsic intrinsic(std::string_view text, float px);
        [[nodiscard]] TextLineLayout lines(std::string_view text, float px, float available_width);
        // Returned view is invalidated by any operation modifying this face's cache.
        // The OpenType instance and its font face must outlive the view.
        [[nodiscard]] std::optional<text::GlyphRunView> shape(std::string_view text, float px);
        [[nodiscard]] text::CacheStats cache_stats() const noexcept;
        void clear_cache() noexcept;
    };
} // namespace arcade::ui::font

namespace arcade::ui::font {
    struct OpenType::State {
        FT_Library library {};
        FT_Face face {};
        hb_font_t* font {};
        text::ShapedRunCache cache {1024, 131072};
        std::uint32_t pixel_size_64 {};
        explicit State(std::string_view file) {
            if (FT_Init_FreeType(&library))
                throw std::runtime_error("FreeType initialization failed");
            const std::string path(file);
            if (FT_New_Face(library, path.c_str(), 0, &face)) {
                FT_Done_FreeType(library);
                library = nullptr;
                throw std::runtime_error("Cannot load font face: " + path);
            }
            font = hb_ft_font_create_referenced(face);
            if (!font) {
                FT_Done_Face(face);
                face = nullptr;
                FT_Done_FreeType(library);
                library = nullptr;
                throw std::runtime_error("Cannot initialize HarfBuzz font");
            }
        }
        ~State() {
            if (font)
                hb_font_destroy(font);
            if (face)
                FT_Done_Face(face);
            if (library)
                FT_Done_FreeType(library);
        }
        void size(float px) {
            if (!std::isfinite(px) || px <= 0 || px > 4096)
                throw std::invalid_argument("Invalid font size");
            if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::max(1.f, std::round(px)))))
                throw std::runtime_error("Cannot set font pixel size");
            hb_ft_font_changed(font);
            pixel_size_64 = static_cast<std::uint32_t>(std::round(px)) * 64u;
        }
        static bool shape_callback(void* context,
                                   std::string_view input,
                                   text::ShapeKey,
                                   std::pmr::vector<text::GlyphMetrics>& glyphs) noexcept {
            try {
                if (input.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                    return false;
                std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer(
                    hb_buffer_create(), &hb_buffer_destroy);
                if (!buffer)
                    return false;
                hb_buffer_add_utf8(buffer.get(),
                                   input.data(),
                                   static_cast<int>(input.size()),
                                   0,
                                   static_cast<int>(input.size()));
                hb_buffer_guess_segment_properties(buffer.get());
                hb_shape(static_cast<State*>(context)->font, buffer.get(), nullptr, 0);
                unsigned count = 0;
                const auto* info = hb_buffer_get_glyph_infos(buffer.get(), &count);
                unsigned position_count = 0;
                const auto* pos = hb_buffer_get_glyph_positions(buffer.get(), &position_count);
                if (count != position_count || (count && (!info || !pos)))
                    return false;
                glyphs.reserve(count);
                for (unsigned i = 0; i < count; ++i) {
                    glyphs.push_back({info[i].codepoint,
                                      info[i].cluster,
                                      static_cast<float>(pos[i].x_advance) / 64.f,
                                      static_cast<float>(pos[i].y_advance) / 64.f,
                                      static_cast<float>(pos[i].x_offset) / 64.f,
                                      static_cast<float>(pos[i].y_offset) / 64.f});
                }
                return true;
            } catch (...) {
                glyphs.clear();
                return false;
            }
        }
        [[nodiscard]] std::optional<text::GlyphRunView> shaped(std::string_view input) {
            const text::ShapeKey key {.face_id = 1, // Cache is owned by one immutable FT_Face.
                                      .face_generation = 1,
                                      .pixel_size_64 = pixel_size_64};
            return cache.get_or_shape(input, key, &shape_callback, this);
        }
        float advance(std::string_view t) {
            if (t.empty())
                return 0;
            const auto result = shaped(t);
            if (!result)
                throw std::runtime_error("OpenType shaping failed");
            return std::max(0.f, result->total_advance);
        }
        float line_height() const {
            return std::max(1.f, static_cast<float>(face->size->metrics.height) / 64.f);
        }
    };
    OpenType::OpenType(std::string_view file_path) : state_(std::make_unique<State>(file_path)) {}
    OpenType::~OpenType() = default;
    OpenType::OpenType(OpenType&&) noexcept = default;
    OpenType& OpenType::operator=(OpenType&&) noexcept = default;
    std::optional<text::GlyphRunView> OpenType::shape(std::string_view value, float px) {
        state_->size(px);
        return state_->shaped(value);
    }
    text::CacheStats OpenType::cache_stats() const noexcept {
        return state_->cache.stats();
    }
    void OpenType::clear_cache() noexcept {
        state_->cache.clear();
    }
    TextIntrinsic OpenType::intrinsic(std::string_view t, float px) {
        state_->size(px);
        float widest = 0, word = 0;
#ifdef ARCADE_UI_HAS_UNICODE
        // UAX #14 break opportunities. This does not implement CSS white-space
        // collapsing or bidi shaping; these require style-aware text runs.
        const auto analysis = unicode::analyze(t);
        for (std::size_t i = 1; i < analysis.breaks.size(); ++i) {
            const auto a = analysis.breaks[i - 1], b = analysis.breaks[i];
            word = std::max(word, state_->advance(t.substr(a, b - a)));
        }
#else
        std::size_t pos = 0;
        while (pos < t.size()) {
            if (t[pos] == ' ' || t[pos] == '\n' || t[pos] == '\t' || t[pos] == '\r') {
                ++pos;
                continue;
            }
            auto e = t.find_first_of(" \t\r\n", pos);
            if (e == std::string_view::npos)
                e = t.size();
            word = std::max(word, state_->advance(t.substr(pos, e - pos)));
            pos = e;
        }
#endif
        std::size_t pos = 0;
        while (pos <= t.size()) {
            auto e = t.find('\n', pos);
            if (e == std::string_view::npos)
                e = t.size();
            widest = std::max(widest, state_->advance(t.substr(pos, e - pos)));
            if (e == t.size())
                break;
            pos = e + 1;
        }
        return {word, widest, state_->line_height()};
    }
    TextLineLayout OpenType::lines(std::string_view t, float px, float available) {
        if (!std::isfinite(available) || available < 0)
            throw std::invalid_argument("Invalid text width");
        state_->size(px);
        float used = 0, maximum = 0;
        std::uint32_t count = 1;
#ifdef ARCADE_UI_HAS_UNICODE
        const auto analysis = unicode::analyze(t);
        for (std::size_t i = 1; i < analysis.breaks.size(); ++i) {
            const auto a = analysis.breaks[i - 1], b = analysis.breaks[i];
            const auto part = t.substr(a, b - a);
            // ICU returns forced boundaries at newline. A proper CSS white-space
            // processor is still needed for collapse/preserve semantics.
            const auto newline = part.find('\n');
            const auto segment = part.substr(0, newline);
            const float w = state_->advance(segment);
            if (used > 0 && used + w > available) {
                maximum = std::max(maximum, used);
                used = 0;
                ++count;
            }
            used += w;
            if (newline != std::string_view::npos) {
                maximum = std::max(maximum, used);
                used = 0;
                ++count;
            }
        }
#else
        std::size_t begin = 0;
        while (begin < t.size()) {
            if (t[begin] == '\n') {
                maximum = std::max(maximum, used);
                used = 0;
                ++count;
                ++begin;
                continue;
            }
            auto end = t.find_first_of(" \t\r\n", begin);
            if (end == std::string_view::npos)
                end = t.size();
            if (end == begin) {
                ++begin;
                continue;
            }
            auto word = t.substr(begin, end - begin);
            const float word_width = state_->advance(word);
            const float space = used > 0 ? state_->advance(" ") : 0.f;
            if (used > 0 && used + space + word_width > available) {
                maximum = std::max(maximum, used);
                used = 0;
                ++count;
            }
            used += (used > 0 ? space : 0) + word_width;
            begin = end;
        }
#endif
        maximum = std::max(maximum, used);
        // Existing Document API requires painted width <= constraint, despite
        // CSS overflow being permitted for unbreakable runs.
        return {std::min(available, maximum), state_->line_height() * count, count};
    }
    TextIntrinsic OpenType::intrinsic_cb(void* context, std::string_view text, float px) noexcept {
        try {
            return static_cast<OpenType*>(context)->intrinsic(text, px);
        } catch (...) {
            return {-1, -1, -1};
        }
    }
    TextLineLayout
    OpenType::line_cb(void* context, std::string_view text, float px, float width) noexcept {
        try {
            return static_cast<OpenType*>(context)->lines(text, px, width);
        } catch (...) {
            return {-1, -1, 0};
        }
    }
    void OpenType::attach(Document& document) {
        document.set_text_measurer(&intrinsic_cb, this);
        document.set_line_measurer(&line_cb, this);
    }
    void OpenType::detach(Document& document) noexcept {
        document.set_line_measurer(nullptr);
        document.set_text_measurer(nullptr);
    }
} // namespace arcade::ui::font
