import arcade.ui.text.cache;
#include <cassert>
#include <cmath>
#include <memory_resource>
#include <string_view>
using namespace arcade::ui::text;
struct Stub {
    int calls {};
};
static bool
shape(void* ptr, std::string_view text, ShapeKey, std::pmr::vector<GlyphMetrics>& result) noexcept {
    ++static_cast<Stub*>(ptr)->calls;
    for (std::size_t i = 0; i < text.size(); ++i)
        result.push_back(
            {static_cast<unsigned char>(text[i]), static_cast<std::uint32_t>(i), 5, 0, 0, 0});
    return true;
}
int main() {
    Stub stub;
    ShapedRunCache cache(2, 8);
    ShapeKey a {.face_id = 1, .face_generation = 1, .pixel_size_64 = 1024};
    auto first = cache.get_or_shape("ab", a, shape, &stub);
    assert(first && first->ids.size() == 2 && first->total_advance == 10);
    auto hit = cache.get_or_shape("ab", a, shape, &stub);
    assert(hit && stub.calls == 1);
    auto b = cache.get_or_shape("xyz", a, shape, &stub);
    assert(b && stub.calls == 2);
    auto c = cache.get_or_shape("q", a, shape, &stub);
    assert(c && cache.stats().evictions == 1);
    assert(!cache.get("ab", a));
    ++a.face_generation;
    assert(cache.get_or_shape("xyz", a, shape, &stub));
    cache.invalidate_face(1);
    assert(cache.stats().entries == 0);
    assert(cache.stats().glyphs == 0);
    assert(!cache.get_or_shape("bad", a, nullptr, nullptr));
    assert(cache.stats().shape_failures == 1);
    // A generation bump is a cache miss even for identical UTF-8.
    ShapeKey new_face {.face_id = 2, .face_generation = 1, .pixel_size_64 = 1024};
    assert(cache.get_or_shape("A", new_face, shape, &stub));
    ++new_face.face_generation;
    assert(!cache.get("A", new_face));
    cache.clear();
    assert(cache.stats().entries == 0);
}
