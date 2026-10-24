module;
#include <unicode/utf8.h>
#include <unicode/uscript.h>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <algorithm>
export module arcade.ui.text.runs;
import arcade.ui.unicode;

export namespace arcade::ui::text {
    // Byte ranges are always offsets into original UTF-8; not glyph indices.
    // A segmentation input for a future HarfBuzz/font-fallback pipeline.
    struct ScriptRun {
        std::uint32_t begin{}, end{};
        std::int32_t script{}; // ICU UScriptCode value; opaque outside backend
        unicode::Direction direction{unicode::Direction::ltr};
    };
    struct VisualLineRuns { std::vector<ScriptRun> runs; };
    [[nodiscard]] std::vector<VisualLineRuns> segment_visual_lines(
        std::string_view utf8, const std::vector<unicode::LineSpan>& lines,
        unicode::Direction base = unicode::Direction::ltr);
}

namespace arcade::ui::text {
    namespace {
        struct Scalar { std::uint32_t begin,end; UScriptCode script; };
        std::vector<Scalar> decode(std::string_view input) {
            if(input.size()>static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
                throw std::length_error("UTF-8 too large for ICU");
            std::vector<Scalar> result;
            int32_t offset=0, length=static_cast<int32_t>(input.size());
            while(offset<length){
                auto begin=offset;
                UChar32 cp=0;
                U8_NEXT(input.data(),offset,length,cp);
                if(cp<0)throw std::invalid_argument("Malformed UTF-8");
                UErrorCode err=U_ZERO_ERROR;
                auto script=uscript_getScript(cp,&err);
                if(U_FAILURE(err))throw std::runtime_error("ICU script detection failed");
                result.push_back({static_cast<std::uint32_t>(begin),static_cast<std::uint32_t>(offset),script});
            }
            return result;
        }
        bool neutral(UScriptCode code){return code==USCRIPT_COMMON || code==USCRIPT_INHERITED || code==USCRIPT_UNKNOWN;}
    }
    std::vector<VisualLineRuns> segment_visual_lines(std::string_view utf8,
        const std::vector<unicode::LineSpan>& lines,unicode::Direction base){
        auto scalars=decode(utf8);
        // Existing bidi implementation validates UTF-8 boundaries and performs
        // ubidi_setLine, rather than reusing paragraph-level visual ordering.
        auto visual=unicode::resolve_visual_lines(utf8,lines,base);
        std::vector<VisualLineRuns> output;
        output.reserve(visual.size());
        for(const auto& line:visual){
            VisualLineRuns shaped;
            for(const auto& bidi:line){
                std::vector<ScriptRun> parts;
                for(const auto& cp:scalars){
                    if(cp.end<=bidi.byte_begin)continue;
                    if(cp.begin>=bidi.byte_end)break;
                    if(cp.begin<bidi.byte_begin || cp.end>bidi.byte_end)
                        throw std::invalid_argument("Bidi run cuts through UTF-8 scalar");
                    auto script=cp.script;
                    if(neutral(script) && !parts.empty())script=static_cast<UScriptCode>(parts.back().script);
                    if(!parts.empty() && (parts.back().script==script || neutral(static_cast<UScriptCode>(parts.back().script)))){
                        if(neutral(static_cast<UScriptCode>(parts.back().script)) && !neutral(script))
                            parts.back().script=script;
                        parts.back().end=cp.end;
                    }else parts.push_back({cp.begin,cp.end,script,bidi.direction});
                }
                // For RTL bidi spans, reverse the order of script runs only;
                // glyph order is the shaper's responsibility.
                if(bidi.direction==unicode::Direction::rtl)std::reverse(parts.begin(),parts.end());
                shaped.runs.insert(shaped.runs.end(),parts.begin(),parts.end());
            }
            output.push_back(std::move(shaped));
        }
        return output;
    }
}
