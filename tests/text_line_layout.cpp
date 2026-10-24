#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string_view>
import arcade.ui;
using namespace arcade::ui;
static TextLineLayout exact(void* context, std::string_view text, float, float available) noexcept {
    ++*static_cast<int*>(context);
    return {std::min(available, text.empty()?0.f:22.f), 40.f, 2};
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    try {
        auto doc=Document::load(argv[1]);
        int called=0;
        doc.set_line_measurer(exact,&called);
        auto r=doc.layout_text(0,50.f);
        if(r.line_count!=2 || r.height!=40.f || called!=1)return 3;
        bool rejected=false;
        try{ (void)doc.layout_text(0,-1.f); }catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)return 4;
        doc.set_line_measurer(nullptr);
        auto fallback=doc.layout_text(0,20.f);
        if(fallback.line_count<1 || fallback.width>20.01f)return 5;
        std::puts("PASS text line measurement API and validation");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 20;}
}
