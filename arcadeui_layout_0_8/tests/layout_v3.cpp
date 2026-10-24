#include <cmath>
#include <cstdio>
#include <exception>
import arcade.ui;
using arcade::ui::Document;
static bool equal(float actual,float expected,const char* label){
    if(std::abs(actual-expected)>0.02f){std::fprintf(stderr,"%s: actual=%f expected=%f\n",label,actual,expected);return false;}
    return true;
}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    try{
        auto grid=Document::load(argv[1]);grid.resize(600,250);
        const auto g=grid.geometry();
        if(g.size()!=5)return 3;
        // Grid: columns 100px, 1fr, 2fr in 600px => 100, 166.667, 333.333.
        // The 3rd child spans the last two columns on a second row.
        if(!equal(g[1].width,100,"fixed track"))return 4;
        if(!equal(g[2].width,166.667f,"fractional track"))return 5;
        if(!equal(g[3].width,333.333f,"weighted fractional track"))return 6;
        if(!equal(g[4].width,266.667f,"span track width"))return 7;
        if(!equal(g[4].x,0,"span track x"))return 8;
        auto flex=Document::load(argv[2]);flex.resize(400,180);
        const auto f=flex.geometry();
        if(f.size()!=3)return 9;
        // Basis 100 and 100, second item grows 1: unused 200 assigned to second.
        if(!equal(f[1].width,100,"basis width"))return 10;
        if(!equal(f[2].width,300,"grow width"))return 11;
        std::puts("PASS grid fr/fixed/span and flex basis/grow");return 0;
    }catch(std::exception const& e){std::fprintf(stderr,"%s\n",e.what());return 20;}
}
