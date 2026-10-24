module;
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <memory_resource>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
export module arcade.ui;

export namespace arcade::ui {
struct Rect {
  float x{}, y{}, width{}, height{};
};
struct Event {
  std::uint32_t target{};
  std::string_view name;
};
struct DrawCommand {
  Rect rect;
  std::uint32_t rgba{};
  std::string_view text;
  std::uint32_t text_rgba{};
  float font_size{};
};
using Handler = void (*)(void *, const Event &) noexcept;
struct Callback {
  Handler function{};
  void *context{};
};
struct Node {
  std::uint32_t parent{}, first_child{}, child_count{}, style{}, text_offset{},
      text_size{}, event_offset{}, event_size{};
};
struct Style {
  std::uint32_t display{}, direction{};
  float width{}, height{}, grow{}, shrink{}, gap{}, padding{}, margin{},
      font_size{};
  std::uint32_t background{}, foreground{}, flags{};
};
static_assert(sizeof(Node) == 32);
static_assert(sizeof(Style) == 52);

class Document {
  std::pmr::vector<std::byte> bytes_;
  std::pmr::vector<Node> nodes_;
  std::pmr::vector<Style> styles_;
  std::pmr::vector<Rect> rectangles_;
  std::pmr::vector<DrawCommand> commands_;
  std::pmr::unordered_map<std::string, Callback> callbacks_;
  std::uint32_t strings_start_{}, strings_size_{};
  float viewport_width_{}, viewport_height_{};
  static std::uint32_t u32(const std::byte *p) noexcept {
    return std::uint32_t(std::to_integer<unsigned char>(p[0])) |
           (std::uint32_t(std::to_integer<unsigned char>(p[1])) << 8) |
           (std::uint32_t(std::to_integer<unsigned char>(p[2])) << 16) |
           (std::uint32_t(std::to_integer<unsigned char>(p[3])) << 24);
  }
  static float f32(const std::byte *p) noexcept {
    auto v = u32(p);
    return std::bit_cast<float>(v);
  }
  [[nodiscard]] std::string_view text(std::uint32_t off,
                                      std::uint32_t len) const {
    if (off > strings_size_ || len > strings_size_ - off)
      throw std::runtime_error("Invalid string offset");
    return {
        reinterpret_cast<const char *>(bytes_.data() + strings_start_ + off),
        len};
  }
  [[nodiscard]] float measure(std::uint32_t id, bool horizontal) const {
    const auto &n = nodes_[id];
    const auto &s = styles_[n.style];
    if (s.display == 3)
      return 0;
    const float fixed = horizontal ? s.width : s.height;
    if (fixed >= 0)
      return fixed;
    const float pad = s.padding * 2;
    if (!n.child_count)
      return pad + (horizontal ? float(n.text_size) * s.font_size * 0.58f
                               : s.font_size * 1.3f);
    float result = 0;
    const bool main = (s.display == 1 && ((s.direction == 0) == horizontal));
    if (main) {
      for (std::uint32_t i = 0; i < n.child_count; ++i)
        result += measure(n.first_child + i, horizontal) +
                  styles_[nodes_[n.first_child + i].style].margin * 2;
      result += s.gap * float(n.child_count - 1);
    } else {
      for (std::uint32_t i = 0; i < n.child_count; ++i)
        result = std::max(
            result, measure(n.first_child + i, horizontal) +
                        styles_[nodes_[n.first_child + i].style].margin * 2);
    }
    return result + pad;
  }
  void layout(std::uint32_t id, Rect area) {
    rectangles_[id] = area;
    const auto &n = nodes_[id];
    const auto &s = styles_[n.style];
    if (s.display == 3 || !n.child_count)
      return;
    const float pad = s.padding;
    const float innerw = std::max(0.f, area.width - 2 * pad),
                innerh = std::max(0.f, area.height - 2 * pad);
    const bool grid = s.display == 2;
    bool horizontal = grid || (s.display == 1 && s.direction == 0);
    float available = horizontal ? innerw : innerh;
    const std::uint32_t columns =
        grid ? std::max(1u, static_cast<std::uint32_t>(
                                std::ceil(std::sqrt(float(n.child_count)))))
             : n.child_count;
    if (grid) {
      float cellw =
          std::max(0.f, (innerw - s.gap * float(columns - 1)) / float(columns));
      float rowy = area.y + pad, rowh = 0;
      for (std::uint32_t i = 0; i < n.child_count; ++i) {
        auto child = n.first_child + i;
        auto &cs = styles_[nodes_[child].style];
        if (i && i % columns == 0) {
          rowy += rowh + s.gap;
          rowh = 0;
        }
        float h = std::max(0.f, measure(child, false));
        rowh = std::max(rowh, h + 2 * cs.margin);
        layout(child,
               {area.x + pad + float(i % columns) * (cellw + s.gap) + cs.margin,
                rowy + cs.margin, std::max(0.f, cellw - 2 * cs.margin), h});
      }
      return;
    }
    float basis = 0, grow_total = 0, shrink_weight = 0;
    for (std::uint32_t i = 0; i < n.child_count; ++i) {
      auto child = n.first_child + i;
      const auto &cs = styles_[nodes_[child].style];
      const float b = measure(child, horizontal);
      basis += b + cs.margin * 2;
      grow_total += std::max(0.f, cs.grow);
      shrink_weight += b * std::max(0.f, cs.shrink);
    }
    if (n.child_count > 1)
      basis += s.gap * float(n.child_count - 1);
    float free = available - basis,
          cursor = (horizontal ? area.x : area.y) + pad;
    for (std::uint32_t i = 0; i < n.child_count; ++i) {
      auto child = n.first_child + i;
      const auto &cs = styles_[nodes_[child].style];
      float b = measure(child, horizontal);
      float main = b;
      if (free > 0 && grow_total > 0)
        main += free * std::max(0.f, cs.grow) / grow_total;
      if (free < 0 && shrink_weight > 0)
        main = std::max(0.f, b + free * (b * std::max(0.f, cs.shrink)) /
                                     shrink_weight);
      float cross =
          std::max(0.f, (horizontal ? innerh : innerw) - cs.margin * 2);
      Rect r = horizontal ? Rect{cursor + cs.margin, area.y + pad + cs.margin,
                                 std::max(0.f, main), cross}
                          : Rect{area.x + pad + cs.margin, cursor + cs.margin,
                                 cross, std::max(0.f, main)};
      if (horizontal && cs.height >= 0)
        r.height = cs.height;
      if (!horizontal && cs.width >= 0)
        r.width = cs.width;
      layout(child, r);
      cursor += main + cs.margin * 2 + s.gap;
    }
  }
  void render() {
    commands_.clear();
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
      auto &n = nodes_[i];
      auto &s = styles_[n.style];
      if (s.display == 3)
        continue;
      commands_.push_back({rectangles_[i], s.background,
                           text(n.text_offset, n.text_size), s.foreground,
                           s.font_size});
    }
  }

public:
  explicit Document(
      std::pmr::memory_resource *memory = std::pmr::get_default_resource())
      : bytes_(memory), nodes_(memory), styles_(memory), rectangles_(memory),
        commands_(memory), callbacks_(memory) {}
  [[nodiscard]] static Document
  load(const std::string &path,
       std::pmr::memory_resource *memory = std::pmr::get_default_resource()) {
    Document result(memory);
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
      throw std::runtime_error("Cannot open arcuxi");
    auto end = stream.tellg();
    if (end < 72 || end > std::streamoff(256 * 1024 * 1024))
      throw std::runtime_error("Invalid image size");
    result.bytes_.resize(static_cast<std::size_t>(end));
    stream.seekg(0);
    stream.read(reinterpret_cast<char *>(result.bytes_.data()), end);
    if (!stream)
      throw std::runtime_error("Truncated image");
    const auto *b = result.bytes_.data();
    const std::array<char, 8> magic = {'A', 'R', 'C',  'U',
                                       'X', 'I', '\0', '\x01'};
    if (std::memcmp(b, magic.data(), 8) != 0)
      throw std::runtime_error("Invalid magic");
    auto version = u32(b + 8), count = u32(b + 12), nbytes = u32(b + 16),
         sbytes = u32(b + 20), strbytes = u32(b + 24), abytes = u32(b + 28);
    if (version != 1 || count > 1000000 || nbytes != count * sizeof(Node) ||
        sbytes != count * sizeof(Style))
      throw std::runtime_error("Invalid header");
    std::uint64_t total = 72ull + nbytes + sbytes + strbytes + abytes;
    if (total != result.bytes_.size())
      throw std::runtime_error("Invalid sections");
    result.strings_start_ = 72 + nbytes + sbytes;
    result.strings_size_ = strbytes;
    for (std::uint32_t i = 0; i < count; ++i) {
      const auto *x = b + 72 + std::size_t(i) * 32;
      Node n{};
      n.parent = u32(x);
      n.first_child = u32(x + 4);
      n.child_count = u32(x + 8);
      n.style = u32(x + 12);
      n.text_offset = u32(x + 16);
      n.text_size = u32(x + 20);
      n.event_offset = u32(x + 24);
      n.event_size = u32(x + 28);
      if (n.style >= count || (n.parent != 0xffffffff && n.parent >= i) ||
          (n.child_count &&
           (n.first_child <= i ||
            std::uint64_t(n.first_child) + n.child_count > count)))
        throw std::runtime_error("Invalid topology");
      result.nodes_.push_back(n);
      (void)result.text(n.text_offset, n.text_size);
      (void)result.text(n.event_offset, n.event_size);
    }
    for (std::uint32_t i = 0; i < count; ++i) {
      auto x = b + 72 + nbytes + std::size_t(i) * 52;
      Style s{};
      s.display = u32(x);
      s.direction = u32(x + 4);
      s.width = f32(x + 8);
      s.height = f32(x + 12);
      s.grow = f32(x + 16);
      s.shrink = f32(x + 20);
      s.gap = f32(x + 24);
      s.padding = f32(x + 28);
      s.margin = f32(x + 32);
      s.font_size = f32(x + 36);
      s.background = u32(x + 40);
      s.foreground = u32(x + 44);
      s.flags = u32(x + 48);
      if (s.display > 3 || s.direction > 1)
        throw std::runtime_error("Invalid style");
      result.styles_.push_back(s);
    }
    result.rectangles_.resize(count);
    return result;
  }
  void resize(float w, float h) {
    if (!std::isfinite(w) || !std::isfinite(h) || w < 0 || h < 0)
      throw std::invalid_argument("Invalid viewport");
    viewport_width_ = w;
    viewport_height_ = h;
    if (nodes_.empty())
      return;
    const auto &root = styles_[nodes_[0].style];
    layout(0, {0, 0, root.width >= 0 ? root.width : w,
               root.height >= 0 ? root.height : h});
    render();
  }
  void write_svg(std::ostream &out) const {
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
        << viewport_width_ << ' ' << viewport_height_ << "\">\n";
    for (const auto &cmd : commands_) {
      const auto color = [&](std::uint32_t c) {
        constexpr char digits[] = "0123456789abcdef";
        char code[8] = {'#', '#', '#', '#', '#', '#', '#', 0};
        for (int j = 0; j < 6; ++j)
          code[j + 1] = digits[(c >> (20 - 4 * j)) & 15];
        return std::string(code);
      };
      if ((cmd.rgba >> 24) != 0) {
        out << "<rect x=\"" << cmd.rect.x << "\" y=\"" << cmd.rect.y
            << "\" width=\"" << cmd.rect.width << "\" height=\""
            << cmd.rect.height << "\" fill=\"" << color(cmd.rgba)
            << "\" fill-opacity=\"" << float(cmd.rgba >> 24) / 255.f
            << "\"/>\n";
      }
      if (!cmd.text.empty()) {
        out << "<text x=\"" << cmd.rect.x << "\" y=\""
            << cmd.rect.y + cmd.font_size << "\" font-size=\"" << cmd.font_size
            << "\" fill=\"" << color(cmd.text_rgba) << "\">";
        for (char ch : cmd.text) {
          switch (ch) {
          case '&':
            out << "&amp;";
            break;
          case '<':
            out << "&lt;";
            break;
          case '>':
            out << "&gt;";
            break;
          case '\"':
            out << "&quot;";
            break;
          default:
            out << ch;
          }
        }
        out << "</text>\n";
      }
    }
    out << "</svg>\n";
  }
  [[nodiscard]] std::span<const DrawCommand> commands() const noexcept {
    return commands_;
  }
  [[nodiscard]] std::span<const Rect> geometry() const noexcept {
    return rectangles_;
  }
  [[nodiscard]] std::size_t node_count() const noexcept {
    return nodes_.size();
  }
  void on(std::string name, Callback callback) {
    callbacks_.insert_or_assign(std::move(name), callback);
  }
  bool dispatch(std::uint32_t node) {
    if (node >= nodes_.size())
      return false;
    auto &n = nodes_[node];
    auto name = text(n.event_offset, n.event_size);
    if (name.empty())
      return false;
    auto it = callbacks_.find(std::string(name));
    if (it == callbacks_.end() || !it->second.function)
      return false;
    it->second.function(it->second.context, Event{node, name});
    return true;
  }
  bool click(float x, float y) {
    for (std::size_t i = nodes_.size(); i > 0; --i) {
      auto id = std::uint32_t(i - 1);
      auto &r = rectangles_[id];
      if (x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height &&
          dispatch(id))
        return true;
    }
    return false;
  }
};
} // namespace arcade::ui
