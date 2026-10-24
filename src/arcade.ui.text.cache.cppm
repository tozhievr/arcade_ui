module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
export module arcade.ui.text.cache;

export namespace arcade::ui::text {
    struct GlyphMetrics {
        std::uint32_t glyph_id {};
        std::uint32_t cluster {}; // UTF-8 byte offset relative to run's beginning
        float advance_x {}, advance_y {};
        float offset_x {}, offset_y {};
    };
    struct ShapeKey {
        std::uint64_t face_id {};
        std::uint64_t face_generation {};
        std::uint32_t pixel_size_64 {};
        std::uint32_t script {};
        std::uint32_t language {};
        std::uint32_t feature_hash {};
        bool rtl {};
        friend bool operator==(const ShapeKey&, const ShapeKey&) = default;
    };
    // Caller is responsible for shaping entire runs (not each code point).
    // Shaper must return UTF-8-relative cluster offsets for every glyph.
    using ShapeCallback = bool (*)(void*,
                                   std::string_view,
                                   ShapeKey,
                                   std::pmr::vector<GlyphMetrics>&) noexcept;
    struct GlyphRunView {
        std::span<const std::uint32_t> ids, clusters;
        std::span<const float> advance_x, advance_y, offset_x, offset_y;
        float total_advance {};
    };
    struct CacheStats {
        std::uint64_t hits {}, misses {}, evictions {}, shape_failures {};
        std::size_t entries {}, glyphs {};
    };
    class ShapedRunCache {
        struct Record {
            ShapeKey key;
            std::pmr::string text;
            std::pmr::vector<std::uint32_t> ids, clusters;
            std::pmr::vector<float> ax, ay, ox, oy;
            std::uint64_t used {};
            float total {};
            explicit Record(std::pmr::memory_resource* r)
                : text(r), ids(r), clusters(r), ax(r), ay(r), ox(r), oy(r) {}
        };
        std::pmr::memory_resource* resource_;
        std::pmr::vector<Record> records_;
        std::size_t max_entries_, max_glyphs_, glyphs_ {};
        std::uint64_t clock_ {};
        CacheStats stats_ {};
        void erase_at(std::size_t index);
        void evict_for(std::size_t incoming);

    public:
        explicit ShapedRunCache(
            std::size_t max_entries = 2048,
            std::size_t max_glyphs = 262144,
            std::pmr::memory_resource* resource = std::pmr::get_default_resource());
        // Views are invalidated by any mutating operation on this cache.
        [[nodiscard]] std::optional<GlyphRunView> get(std::string_view text, ShapeKey key) noexcept;
        [[nodiscard]] std::optional<GlyphRunView>
        get_or_shape(std::string_view text, ShapeKey key, ShapeCallback callback, void* context);
        void invalidate_face(std::uint64_t face_id);
        void clear() noexcept;
        [[nodiscard]] CacheStats stats() const noexcept;
    };
} // namespace arcade::ui::text

namespace arcade::ui::text {
    ShapedRunCache::ShapedRunCache(std::size_t entries,
                                   std::size_t glyphs,
                                   std::pmr::memory_resource* resource)
        : resource_(resource ? resource : std::pmr::get_default_resource()), records_(resource_),
          max_entries_(entries), max_glyphs_(glyphs) {}
    void ShapedRunCache::erase_at(std::size_t i) {
        glyphs_ -= records_[i].ids.size();
        records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(i));
        ++stats_.evictions;
    }
    void ShapedRunCache::evict_for(std::size_t incoming) {
        while (!records_.empty() &&
               (records_.size() >= max_entries_ || glyphs_ > max_glyphs_ - incoming)) {
            auto it =
                std::min_element(records_.begin(),
                                 records_.end(),
                                 [](const auto& a, const auto& b) { return a.used < b.used; });
            erase_at(static_cast<std::size_t>(it - records_.begin()));
        }
    }
    std::optional<GlyphRunView> ShapedRunCache::get(std::string_view text, ShapeKey key) noexcept {
        for (auto& record : records_)
            if (record.key == key && record.text == text) {
                record.used = ++clock_;
                ++stats_.hits;
                return GlyphRunView {record.ids,
                                     record.clusters,
                                     record.ax,
                                     record.ay,
                                     record.ox,
                                     record.oy,
                                     record.total};
            }
        ++stats_.misses;
        return std::nullopt;
    }
    std::optional<GlyphRunView> ShapedRunCache::get_or_shape(std::string_view text,
                                                             ShapeKey key,
                                                             ShapeCallback callback,
                                                             void* context) {
        if (auto hit = get(text, key))
            return hit;
        if (!callback) {
            ++stats_.shape_failures;
            return std::nullopt;
        }
        std::pmr::vector<GlyphMetrics> glyphs(resource_);
        if (!callback(context, text, key, glyphs)) {
            ++stats_.shape_failures;
            return std::nullopt;
        }
        if (glyphs.size() > max_glyphs_ || max_entries_ == 0) {
            ++stats_.shape_failures;
            return std::nullopt;
        }
        float sum = 0;
        for (const auto& g : glyphs) {
            if (g.cluster >= text.size() && !text.empty()) {
                ++stats_.shape_failures;
                return std::nullopt;
            }
            if (!text.empty() && (static_cast<unsigned char>(text[g.cluster]) & 0xC0u) == 0x80u) {
                ++stats_.shape_failures;
                return std::nullopt;
            }
            if ((text.empty() && !glyphs.empty()) || !std::isfinite(g.advance_x) ||
                !std::isfinite(g.advance_y) || !std::isfinite(g.offset_x) ||
                !std::isfinite(g.offset_y)) {
                ++stats_.shape_failures;
                return std::nullopt;
            }
            sum += g.advance_x;
        }
        if (!std::isfinite(sum)) {
            ++stats_.shape_failures;
            return std::nullopt;
        }
        Record record(resource_);
        record.key = key;
        record.text.assign(text.begin(), text.end());
        record.used = ++clock_;
        record.total = sum;
        record.ids.reserve(glyphs.size());
        record.clusters.reserve(glyphs.size());
        record.ax.reserve(glyphs.size());
        record.ay.reserve(glyphs.size());
        record.ox.reserve(glyphs.size());
        record.oy.reserve(glyphs.size());
        for (const auto& g : glyphs) {
            record.ids.push_back(g.glyph_id);
            record.clusters.push_back(g.cluster);
            record.ax.push_back(g.advance_x);
            record.ay.push_back(g.advance_y);
            record.ox.push_back(g.offset_x);
            record.oy.push_back(g.offset_y);
        }
        evict_for(glyphs.size());
        glyphs_ += glyphs.size();
        records_.push_back(std::move(record));
        auto& saved = records_.back();
        return GlyphRunView {
            saved.ids, saved.clusters, saved.ax, saved.ay, saved.ox, saved.oy, saved.total};
    }
    void ShapedRunCache::invalidate_face(std::uint64_t face) {
        for (std::size_t i = 0; i < records_.size();) {
            if (records_[i].key.face_id == face)
                erase_at(i);
            else
                ++i;
        }
    }
    void ShapedRunCache::clear() noexcept {
        records_.clear();
        glyphs_ = 0;
        clock_ = 0;
    }
    CacheStats ShapedRunCache::stats() const noexcept {
        auto s = stats_;
        s.entries = records_.size();
        s.glyphs = glyphs_;
        return s;
    }
} // namespace arcade::ui::text
