module;
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/ustring.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
export module arcade.ui.unicode;

export namespace arcade::ui::unicode {
    enum class Direction : std::uint8_t { ltr, rtl };
    struct VisualRun {
        std::uint32_t byte_begin {};
        std::uint32_t byte_end {};
        Direction direction {};
    };
    struct TextAnalysis {
        // UTF-8 byte offsets; includes 0 and input.size(). ICU line boundaries
        // are legal UAX #14 opportunities, not a CSS white-space policy.
        std::vector<std::uint32_t> breaks;
        // Entire-paragraph UAX #9 visual runs, ordered for left-to-right display.
        // CSS line layout must call bidi again on each *resolved line*.
        std::vector<VisualRun> visual_runs;
    };
    struct LineSpan {
        std::uint32_t begin {}, end {};
    };
    [[nodiscard]] std::vector<std::vector<VisualRun>>
    resolve_visual_lines(std::string_view utf8,
                         const std::vector<LineSpan>& lines,
                         Direction direction = Direction::ltr);
    [[nodiscard]] TextAnalysis analyze(std::string_view utf8,
                                       Direction paragraph_direction = Direction::ltr);
    // UAX #29 extended grapheme-cluster boundaries in original UTF-8 byte offsets.
    [[nodiscard]] std::vector<std::uint32_t> grapheme_boundaries(std::string_view utf8);
} // namespace arcade::ui::unicode

namespace arcade::ui::unicode {
    TextAnalysis analyze(std::string_view input, Direction paragraph_direction) {
        if (input.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
            throw std::length_error("Unicode input exceeds ICU's maximum length");
        UErrorCode ec = U_ZERO_ERROR;
        int32_t len = 0;
        u_strFromUTF8(nullptr, 0, &len, input.data(), static_cast<int32_t>(input.size()), &ec);
        if (ec != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(ec))
            throw std::invalid_argument("Invalid UTF-8 input");
        ec = U_ZERO_ERROR;
        std::vector<UChar> utf16(static_cast<std::size_t>(len) + 1);
        u_strFromUTF8(utf16.data(),
                      static_cast<int32_t>(utf16.size()),
                      &len,
                      input.data(),
                      static_cast<int32_t>(input.size()),
                      &ec);
        if (U_FAILURE(ec))
            throw std::invalid_argument("Invalid UTF-8 input");
        // A full positional mapping avoids treating UTF-16 offsets as UTF-8 offsets.
        std::vector<std::uint32_t> to_byte(static_cast<std::size_t>(len) + 1);
        std::size_t p = 0;
        int32_t u = 0;
        while (p < input.size()) {
            const unsigned char lead = static_cast<unsigned char>(input[p]);
            int bytes = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            const bool supplementary = bytes == 4;
            const auto index = static_cast<std::uint32_t>(p);
            to_byte[u++] = index;
            if (supplementary)
                to_byte[u++] = index;
            p += static_cast<std::size_t>(bytes);
        }
        to_byte[len] = static_cast<std::uint32_t>(input.size());
        ec = U_ZERO_ERROR;
        UBreakIterator* iterator = ubrk_open(UBRK_LINE, "und", utf16.data(), len, &ec);
        if (U_FAILURE(ec) || !iterator)
            throw std::runtime_error("ICU line breaker initialization failed");
        TextAnalysis result;
        for (int32_t boundary = ubrk_first(iterator); boundary != UBRK_DONE;
             boundary = ubrk_next(iterator))
            result.breaks.push_back(to_byte[boundary]);
        ubrk_close(iterator);
        ec = U_ZERO_ERROR;
        UBiDi* bidi = ubidi_openSized(len, 0, &ec);
        if (U_FAILURE(ec) || !bidi)
            throw std::runtime_error("ICU bidi initialization failed");
        const UBiDiLevel base = paragraph_direction == Direction::rtl ? UBIDI_RTL : UBIDI_LTR;
        ubidi_setPara(bidi, utf16.data(), len, base, nullptr, &ec);
        if (U_FAILURE(ec)) {
            ubidi_close(bidi);
            throw std::runtime_error("ICU bidi resolution failed");
        }
        int32_t runs = ubidi_countRuns(bidi, &ec);
        if (U_FAILURE(ec)) {
            ubidi_close(bidi);
            throw std::runtime_error("ICU bidi run resolution failed");
        }
        for (int32_t i = 0; i < runs; ++i) {
            int32_t start = 0, run_len = 0;
            const UBiDiDirection dir = ubidi_getVisualRun(bidi, i, &start, &run_len);
            result.visual_runs.push_back({to_byte[start],
                                          to_byte[start + run_len],
                                          dir == UBIDI_RTL ? Direction::rtl : Direction::ltr});
        }
        ubidi_close(bidi);
        return result;
    }
} // namespace arcade::ui::unicode

namespace arcade::ui::unicode {
    std::vector<std::vector<VisualRun>> resolve_visual_lines(std::string_view input,
                                                             const std::vector<LineSpan>& lines,
                                                             Direction direction) {
        if (input.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
            throw std::length_error("Unicode input too long");
        UErrorCode ec = U_ZERO_ERROR;
        int32_t len = 0;
        u_strFromUTF8(nullptr, 0, &len, input.data(), static_cast<int32_t>(input.size()), &ec);
        if (ec != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(ec))
            throw std::invalid_argument("Invalid UTF-8");
        ec = U_ZERO_ERROR;
        std::vector<UChar> units(static_cast<std::size_t>(len) + 1);
        u_strFromUTF8(units.data(),
                      static_cast<int32_t>(units.size()),
                      &len,
                      input.data(),
                      static_cast<int32_t>(input.size()),
                      &ec);
        if (U_FAILURE(ec))
            throw std::invalid_argument("Invalid UTF-8");
        // UTF-16 boundary indices to original UTF-8 offsets; a surrogate interior
        // is not a valid line boundary.
        std::vector<std::uint32_t> to_byte(static_cast<std::size_t>(len) + 1);
        std::vector<int32_t> to_u16(input.size() + 1, -1);
        std::size_t b = 0;
        int32_t u = 0;
        while (b < input.size()) {
            auto lead = static_cast<unsigned char>(input[b]);
            int n = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            to_byte[u] = static_cast<std::uint32_t>(b);
            to_u16[b] = u;
            ++u;
            if (n == 4) {
                to_byte[u] = static_cast<std::uint32_t>(b);
                ++u;
            }
            b += static_cast<std::size_t>(n);
        }
        to_byte[len] = static_cast<std::uint32_t>(input.size());
        to_u16[input.size()] = len;
        ec = U_ZERO_ERROR;
        UBiDi* para = ubidi_openSized(len, 0, &ec);
        if (U_FAILURE(ec) || !para)
            throw std::runtime_error("ICU ubidi_openSized failed");
        const UBiDiLevel base = direction == Direction::rtl ? UBIDI_RTL : UBIDI_LTR;
        ubidi_setPara(para, units.data(), len, base, nullptr, &ec);
        if (U_FAILURE(ec)) {
            ubidi_close(para);
            throw std::runtime_error("ICU ubidi_setPara failed");
        }
        std::vector<std::vector<VisualRun>> output;
        try {
            for (auto span : lines) {
                if (span.begin > span.end || span.end > input.size() || to_u16[span.begin] < 0 ||
                    to_u16[span.end] < 0)
                    throw std::invalid_argument(
                        "Line ranges must be on UTF-8 code point boundaries");
                int32_t first = to_u16[span.begin], last = to_u16[span.end];
                if (first == last) {
                    output.emplace_back();
                    continue;
                }
                // ICU setLine requires an existing paragraph bidi object.
                ec = U_ZERO_ERROR;
                UBiDi* line = ubidi_openSized(last - first, 0, &ec);
                if (U_FAILURE(ec) || !line)
                    throw std::runtime_error("ICU line bidi allocation failed");
                ubidi_setLine(para, first, last, line, &ec);
                if (U_FAILURE(ec)) {
                    ubidi_close(line);
                    throw std::runtime_error("ICU ubidi_setLine failed");
                }
                int32_t count = ubidi_countRuns(line, &ec);
                if (U_FAILURE(ec)) {
                    ubidi_close(line);
                    throw std::runtime_error("ICU ubidi_countRuns failed");
                }
                std::vector<VisualRun> runs;
                for (int32_t i = 0; i < count; ++i) {
                    int32_t start = 0, extent = 0;
                    auto order = ubidi_getVisualRun(line, i, &start, &extent);
                    runs.push_back({to_byte[first + start],
                                    to_byte[first + start + extent],
                                    order == UBIDI_RTL ? Direction::rtl : Direction::ltr});
                }
                ubidi_close(line);
                output.push_back(std::move(runs));
            }
        } catch (...) {
            ubidi_close(para);
            throw;
        }
        ubidi_close(para);
        return output;
    }
} // namespace arcade::ui::unicode

namespace arcade::ui::unicode {
    std::vector<std::uint32_t> grapheme_boundaries(std::string_view input) {
        if (input.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
            throw std::length_error("Unicode input exceeds ICU maximum length");
        UErrorCode error = U_ZERO_ERROR;
        int32_t length = 0;
        u_strFromUTF8(
            nullptr, 0, &length, input.data(), static_cast<int32_t>(input.size()), &error);
        if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error))
            throw std::invalid_argument("Invalid UTF-8 input");
        error = U_ZERO_ERROR;
        std::vector<UChar> units(static_cast<std::size_t>(length) + 1);
        u_strFromUTF8(units.data(),
                      static_cast<int32_t>(units.size()),
                      &length,
                      input.data(),
                      static_cast<int32_t>(input.size()),
                      &error);
        if (U_FAILURE(error))
            throw std::invalid_argument("Invalid UTF-8 input");
        std::vector<std::uint32_t> to_byte(static_cast<std::size_t>(length) + 1);
        std::size_t byte = 0;
        int32_t offset = 0;
        while (byte < input.size()) {
            const unsigned char lead = static_cast<unsigned char>(input[byte]);
            const int size = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            to_byte[offset++] = static_cast<std::uint32_t>(byte);
            if (size == 4)
                to_byte[offset++] = static_cast<std::uint32_t>(byte);
            byte += static_cast<std::size_t>(size);
        }
        to_byte[length] = static_cast<std::uint32_t>(input.size());
        error = U_ZERO_ERROR;
        UBreakIterator* iterator = ubrk_open(UBRK_CHARACTER, "und", units.data(), length, &error);
        if (U_FAILURE(error) || !iterator)
            throw std::runtime_error("ICU grapheme breaker initialization failed");
        std::vector<std::uint32_t> boundaries;
        for (int32_t pos = ubrk_first(iterator); pos != UBRK_DONE; pos = ubrk_next(iterator))
            boundaries.push_back(to_byte[pos]);
        ubrk_close(iterator);
        return boundaries;
    }
} // namespace arcade::ui::unicode
