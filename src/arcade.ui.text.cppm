module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
export module arcade.ui.text;
import arcade.ui.unicode;

export namespace arcade::ui::text {
    enum class WhiteSpace : std::uint8_t { normal, nowrap, pre, pre_wrap, pre_line, break_spaces };
    enum class OverflowWrap : std::uint8_t { normal, anywhere, break_word };
    struct Style {
        WhiteSpace white_space {WhiteSpace::normal};
        OverflowWrap overflow_wrap {OverflowWrap::normal};
        unicode::Direction direction {unicode::Direction::ltr};
    };
    // The callback measures a complete text fragment. A real shaper must respect
    // script/direction/font runs. Widths cannot simply be summed across Arabic
    // or Indic shaping boundaries without reshaping.
    using Measure = float (*)(void*, std::string_view) noexcept;
    struct Fragment {
        std::uint32_t begin {}, end {};
    }; // original UTF-8 offsets
    struct Line {
        std::vector<Fragment> fragments;
        float width {};
        bool forced {};
        std::vector<unicode::VisualRun> visual_runs;
    };
    struct Layout {
        std::vector<Line> lines;
    };
    [[nodiscard]] Layout
    fit(std::string_view input, float available_width, Style style, Measure measure, void* context);
} // namespace arcade::ui::text

namespace arcade::ui::text {
    namespace {
        bool collapse(WhiteSpace w) {
            return w == WhiteSpace::normal || w == WhiteSpace::nowrap || w == WhiteSpace::pre_line;
        }
        bool preserve_newlines(WhiteSpace w) {
            return w == WhiteSpace::pre || w == WhiteSpace::pre_wrap || w == WhiteSpace::pre_line ||
                   w == WhiteSpace::break_spaces;
        }
        bool wrap(WhiteSpace w) {
            return w != WhiteSpace::nowrap && w != WhiteSpace::pre;
        }
        bool blank(char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
        }
    } // namespace
    Layout fit(std::string_view input, float width, Style style, Measure measure, void* ctx) {
        if (!measure)
            throw std::invalid_argument("Text width callback required");
        if (!std::isfinite(width) || width < 0)
            throw std::invalid_argument("Invalid available width");
        if (input.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Text too long");
        // Validates UTF-8 and supplies normative UAX #14 soft break positions.
        auto analyzed = unicode::analyze(input, style.direction);
        std::vector<bool> allowed(input.size() + 1, false);
        for (auto b : analyzed.breaks)
            allowed[b] = true;
        Layout output;
        Line current;
        std::string visible;
        // ICU UAX #29: emergency wrapping must never split combining sequences,
        // emoji modifiers or ZWJ sequences. Iterate graphemes, not code points.
        const auto graphemes = unicode::grapheme_boundaries(input);
        std::size_t grapheme_index = 0;
        std::uint32_t p = 0;
        bool previous_space = false;
        auto push = [&](bool forced) {
            current.forced = forced;
            output.lines.push_back(std::move(current));
            current = Line {};
            visible.clear();
            previous_space = false;
        };
        auto add = [&](std::uint32_t a, std::uint32_t b, std::string_view display) {
            if (a == b)
                return;
            visible.append(display);
            current.fragments.push_back({a, b});
            current.width = measure(ctx, visible);
            if (!std::isfinite(current.width) || current.width < 0)
                throw std::runtime_error("Invalid measured width");
        };
        while (p < input.size()) {
            const auto start = p;
            const auto end = graphemes[++grapheme_index];
            const char c = input[p];
            p = end;
            const bool ascii_ws = (end == start + 1 && blank(c));
            const bool hard = ((ascii_ws && (c == '\n' || c == '\r')) ||
                               input.substr(start, end - start) == "\r\n") &&
                              preserve_newlines(style.white_space);
            if (hard) {
                if (c == '\r' && p < input.size() && input[p] == '\n') {
                    ++p;
                    ++grapheme_index;
                }
                push(true);
                continue;
            }
            const bool compress = collapse(style.white_space) && ascii_ws;
            if (compress && previous_space)
                continue;
            const std::string_view display =
                compress ? std::string_view(" ") : input.substr(start, end - start);
            // Ordinary CSS wraps at legal UAX #14 boundaries; forced wrapping
            // with anywhere/break-word may break at codepoint boundaries.
            const bool allow_emergency = style.overflow_wrap != OverflowWrap::normal;
            if (wrap(style.white_space) && !visible.empty()) {
                std::string candidate = visible;
                candidate.append(display);
                const float size = measure(ctx, candidate);
                if (!std::isfinite(size) || size < 0)
                    throw std::runtime_error("Invalid measured width");
                if (size > width) {
                    // The last legal break is found among fragments, measured as
                    // a complete prefix to avoid summing independently shaped runs.
                    std::size_t cut = 0;
                    for (std::size_t i = 0; i < current.fragments.size(); ++i)
                        if (allowed[current.fragments[i].end])
                            cut = i + 1;
                    if (cut > 0) {
                        const auto tail = std::vector<Fragment>(current.fragments.begin() + cut,
                                                                current.fragments.end());
                        push(false);
                        for (const auto& f : tail)
                            add(f.begin, f.end, input.substr(f.begin, f.end - f.begin));
                    } else if (allow_emergency) {
                        push(false);
                    }
                }
            }
            add(start, end, display);
            previous_space = compress;
        }
        push(false);
        // Resolve visual order after breaks. Fragment spans remain tied to the
        // original UTF-8 input, including whitespace-collapsed source ranges.
        std::vector<unicode::LineSpan> spans;
        spans.reserve(output.lines.size());
        for (const auto& l : output.lines) {
            if (l.fragments.empty()) {
                spans.push_back({0, 0});
                continue;
            }
            spans.push_back({l.fragments.front().begin, l.fragments.back().end});
        }
        // ICU setLine requires ordered, non-overlapping slices of the paragraph.
        auto runs = unicode::resolve_visual_lines(input, spans, style.direction);
        for (std::size_t i = 0; i < output.lines.size(); ++i)
            output.lines[i].visual_runs = std::move(runs[i]);
        return output;
    }
} // namespace arcade::ui::text
