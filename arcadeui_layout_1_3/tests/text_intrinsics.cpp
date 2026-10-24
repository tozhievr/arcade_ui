#include <cstdint>
#include <string_view>
#include <cmath>
#include <cstdio>
#include <exception>
import arcade.ui;

using arcade::ui::TextIntrinsic;
static TextIntrinsic fixed(void* ctx,std::string_view text,float size) noexcept {
    auto* counter=static_cast<int*>(ctx);
    ++*counter;
    return {text.empty()?0.f:25.f,text.empty()?0.f:70.f,size*1.5f};
}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    try{
        auto doc=arcade::ui::Document::load(argv[1]);
        std::size_t nonempty=0;
        for(std::uint32_t i=0;i<doc.node_count();++i){
            auto m=doc.intrinsic_text(i);
            if(!(m.min_content>=0 && m.max_content>=m.min_content && m.line_height>=0))return 3;
            if(m.max_content>0)++nonempty;
        }
        if(nonempty==0)return 4;
        int calls=0;
        doc.set_text_measurer(fixed,&calls);
        doc.resize(1024,768);
        if(calls==0)return 5;
        const int initial=calls;
        for(std::uint32_t i=0;i<doc.node_count();++i){
            (void)doc.intrinsic_text(i);
            (void)doc.intrinsic_text(i);
        }
        if(calls!=static_cast<int>(doc.node_count()))return 6;
        (void)initial;
        doc.resize(1024,768);
        if(calls!=static_cast<int>(doc.node_count()))return 7;
        auto unicode=arcade::ui::Document::load(argv[2]);
        bool found_breakable=false;
        bool found_cjk=false;
        for(std::uint32_t i=0;i<unicode.node_count();++i){
            const auto m=unicode.intrinsic_text(i);
            if(m.max_content>m.min_content+1.f)found_breakable=true;
            if(m.max_content>=50.f&&m.min_content>=35.f)found_cjk=true;
        }
        if(!found_breakable || !found_cjk)return 8;
        std::puts("PASS text metrics callback, cache, UTF-8 advances and break opportunities");
        return 0;
    }catch(const std::exception& ex){std::fprintf(stderr,"%s\n",ex.what());return 20;}
}
