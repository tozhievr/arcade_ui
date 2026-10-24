module;
#include <unicode/ubrk.h>
#include <unicode/ubidi.h>
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
        std::uint32_t byte_begin{};
        std::uint32_t byte_end{};
        Direction direction{};
    };
    struct TextAnalysis {
        // UTF-8 byte offsets; includes 0 and input.size(). ICU line boundaries
        // are legal UAX #14 opportunities, not a CSS white-space policy.
        std::vector<std::uint32_t> breaks;
        // Entire-paragraph UAX #9 visual runs, ordered for left-to-right display.
        // CSS line layout must call bidi again on each *resolved line*.
        std::vector<VisualRun> visual_runs;
    };
    [[nodiscard]] TextAnalysis analyze(std::string_view utf8,
        Direction paragraph_direction = Direction::ltr);
}

namespace arcade::ui::unicode {
    TextAnalysis analyze(std::string_view input,Direction paragraph_direction) {
        if(input.size()>static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
            throw std::length_error("Unicode input exceeds ICU's maximum length");
        UErrorCode ec=U_ZERO_ERROR;
        int32_t len=0;
        u_strFromUTF8(nullptr,0,&len,input.data(),static_cast<int32_t>(input.size()),&ec);
        if(ec!=U_BUFFER_OVERFLOW_ERROR && U_FAILURE(ec))
            throw std::invalid_argument("Invalid UTF-8 input");
        ec=U_ZERO_ERROR;
        std::vector<UChar> utf16(static_cast<std::size_t>(len)+1);
        u_strFromUTF8(utf16.data(),static_cast<int32_t>(utf16.size()),&len,
            input.data(),static_cast<int32_t>(input.size()),&ec);
        if(U_FAILURE(ec))throw std::invalid_argument("Invalid UTF-8 input");
        // A full positional mapping avoids treating UTF-16 offsets as UTF-8 offsets.
        std::vector<std::uint32_t> to_byte(static_cast<std::size_t>(len)+1);
        std::size_t p=0;int32_t u=0;
        while(p<input.size()) {
            const unsigned char lead=static_cast<unsigned char>(input[p]);
            int bytes=lead<0x80?1:lead<0xE0?2:lead<0xF0?3:4;
            const bool supplementary=bytes==4;
            const auto index=static_cast<std::uint32_t>(p);
            to_byte[u++]=index;
            if(supplementary)to_byte[u++]=index;
            p+=static_cast<std::size_t>(bytes);
        }
        to_byte[len]=static_cast<std::uint32_t>(input.size());
        ec=U_ZERO_ERROR;
        UBreakIterator* iterator=ubrk_open(UBRK_LINE,"und",utf16.data(),len,&ec);
        if(U_FAILURE(ec)||!iterator)throw std::runtime_error("ICU line breaker initialization failed");
        TextAnalysis result;
        for(int32_t boundary=ubrk_first(iterator);boundary!=UBRK_DONE;boundary=ubrk_next(iterator))
            result.breaks.push_back(to_byte[boundary]);
        ubrk_close(iterator);
        ec=U_ZERO_ERROR;
        UBiDi* bidi=ubidi_openSized(len,0,&ec);
        if(U_FAILURE(ec)||!bidi)throw std::runtime_error("ICU bidi initialization failed");
        const UBiDiLevel base=paragraph_direction==Direction::rtl?UBIDI_RTL:UBIDI_LTR;
        ubidi_setPara(bidi,utf16.data(),len,base,nullptr,&ec);
        if(U_FAILURE(ec)){ubidi_close(bidi);throw std::runtime_error("ICU bidi resolution failed");}
        int32_t runs=ubidi_countRuns(bidi,&ec);
        if(U_FAILURE(ec)){ubidi_close(bidi);throw std::runtime_error("ICU bidi run resolution failed");}
        for(int32_t i=0;i<runs;++i){
            int32_t start=0,run_len=0;
            const UBiDiDirection dir=ubidi_getVisualRun(bidi,i,&start,&run_len);
            result.visual_runs.push_back({to_byte[start],to_byte[start+run_len],
                dir==UBIDI_RTL?Direction::rtl:Direction::ltr});
        }
        ubidi_close(bidi);
        return result;
    }
}
