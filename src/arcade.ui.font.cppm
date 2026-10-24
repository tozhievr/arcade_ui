module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
export module arcade.ui.font;
export import arcade.ui;

export namespace arcade::ui::font {
    // Deliberately small, standalone OpenType backend. The implementation owns
    // FT_Face and hb_font_t; callers must keep this object alive while attached
    // to a Document. Not thread-safe (FT_Face pixel size is mutable).
    class OpenType {
        struct State;
        std::unique_ptr<State> state_;
        static TextIntrinsic intrinsic_cb(void*,std::string_view,float) noexcept;
        static TextLineLayout line_cb(void*,std::string_view,float,float) noexcept;
    public:
        explicit OpenType(std::string_view file_path);
        ~OpenType();
        OpenType(const OpenType&)=delete;
        OpenType& operator=(const OpenType&)=delete;
        OpenType(OpenType&&) noexcept;
        OpenType& operator=(OpenType&&) noexcept;
        void attach(Document& document);
        void detach(Document& document) noexcept;
        [[nodiscard]] TextIntrinsic intrinsic(std::string_view text,float px);
        [[nodiscard]] TextLineLayout lines(std::string_view text,float px,float available_width);
    };
}

namespace arcade::ui::font {
    struct OpenType::State {
        FT_Library library{};
        FT_Face face{};
        hb_font_t* font{};
        explicit State(std::string_view file) {
            if(FT_Init_FreeType(&library))throw std::runtime_error("FreeType initialization failed");
            const std::string path(file);
            if(FT_New_Face(library,path.c_str(),0,&face)){FT_Done_FreeType(library);library=nullptr;throw std::runtime_error("Cannot load font face: "+path);}
            font=hb_ft_font_create_referenced(face);
            if(!font){FT_Done_Face(face);face=nullptr;FT_Done_FreeType(library);library=nullptr;throw std::runtime_error("Cannot initialize HarfBuzz font");}
        }
        ~State(){if(font)hb_font_destroy(font);if(face)FT_Done_Face(face);if(library)FT_Done_FreeType(library);}
        void size(float px){
            if(!std::isfinite(px)||px<=0||px>4096)throw std::invalid_argument("Invalid font size");
            if(FT_Set_Pixel_Sizes(face,0,static_cast<FT_UInt>(std::max(1.f,std::round(px)))))
                throw std::runtime_error("Cannot set font pixel size");
            hb_ft_font_changed(font);
        }
        float advance(std::string_view t) {
            if(t.empty())return 0;
            hb_buffer_t* b=hb_buffer_create();
            if(!b)throw std::bad_alloc();
            hb_buffer_add_utf8(b,t.data(),static_cast<int>(t.size()),0,static_cast<int>(t.size()));
            hb_buffer_guess_segment_properties(b);
            hb_shape(font,b,nullptr,0);
            unsigned count=0;
            const auto* pos=hb_buffer_get_glyph_positions(b,&count);
            std::int64_t sum=0;
            for(unsigned i=0;i<count;++i)sum+=pos[i].x_advance;
            hb_buffer_destroy(b);
            return std::max(0.f,static_cast<float>(sum)/64.f);
        }
        float line_height() const {
            return std::max(1.f,static_cast<float>(face->size->metrics.height)/64.f);
        }
    };
    OpenType::OpenType(std::string_view file_path):state_(std::make_unique<State>(file_path)){}
    OpenType::~OpenType()=default;
    OpenType::OpenType(OpenType&&) noexcept=default;
    OpenType& OpenType::operator=(OpenType&&) noexcept=default;
    TextIntrinsic OpenType::intrinsic(std::string_view t,float px){
        state_->size(px);
        float widest=0,word=0;
        // Split at ASCII white space for the initial wrapping profile.
        // Each word is shaped as a unit, preserving intra-word kerning.
        std::size_t pos=0;
        while(pos<t.size()){
            if(t[pos]==' '||t[pos]=='\n'||t[pos]=='\t'||t[pos]=='\r'){++pos;continue;}
            auto e=t.find_first_of(" \t\r\n",pos);
            if(e==std::string_view::npos)e=t.size();
            word=std::max(word,state_->advance(t.substr(pos,e-pos)));
            pos=e;
        }
        pos=0;
        while(pos<=t.size()){
            auto e=t.find('\n',pos);
            if(e==std::string_view::npos)e=t.size();
            widest=std::max(widest,state_->advance(t.substr(pos,e-pos)));
            if(e==t.size())break;
            pos=e+1;
        }
        return {word,widest,state_->line_height()};
    }
    TextLineLayout OpenType::lines(std::string_view t,float px,float available){
        if(!std::isfinite(available)||available<0)throw std::invalid_argument("Invalid text width");
        state_->size(px);
        float used=0,maximum=0;
        std::uint32_t count=1;
        // Greedy wrapping at ASCII whitespace. A UAX#14 breaker is needed for
        // full CSS text layout; this backend implements accurate glyph advances.
        std::size_t begin=0;
        while(begin<t.size()){
            if(t[begin]=='\n'){
                maximum=std::max(maximum,used);used=0;++count;++begin;continue;
            }
            auto end=t.find_first_of(" \t\r\n",begin);
            if(end==std::string_view::npos)end=t.size();
            if(end==begin){++begin;continue;}
            auto word=t.substr(begin,end-begin);
            const float word_width=state_->advance(word);
            const float space=used>0?state_->advance(" "):0.f;
            if(used>0 && used+space+word_width>available){
                maximum=std::max(maximum,used);used=0;++count;
            }
            // Overflow for unbreakable words is allowed by CSS; report
            // clamped painted width for the current Document validation API.
            used=std::min(available,used+(used>0?space:0)+word_width);
            begin=end;
        }
        maximum=std::max(maximum,used);
        return {maximum,state_->line_height()*count,count};
    }
    TextIntrinsic OpenType::intrinsic_cb(void* context,std::string_view text,float px) noexcept {
        try{return static_cast<OpenType*>(context)->intrinsic(text,px);}
        catch(...){return {-1,-1,-1};}
    }
    TextLineLayout OpenType::line_cb(void* context,std::string_view text,float px,float width) noexcept {
        try{return static_cast<OpenType*>(context)->lines(text,px,width);}
        catch(...){return {-1,-1,0};}
    }
    void OpenType::attach(Document& document){
        document.set_text_measurer(&intrinsic_cb,this);
        document.set_line_measurer(&line_cb,this);
    }
    void OpenType::detach(Document& document) noexcept {
        document.set_line_measurer(nullptr);
        document.set_text_measurer(nullptr);
    }
}
