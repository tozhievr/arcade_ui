#include <cmath>
#include <cstdio>
#include <exception>
import arcade.ui;
using arcade::ui::Document;
static bool same(Document const& a, Document const& b){
    auto x=a.geometry(),y=b.geometry();
    if(x.size()!=y.size())return false;
    for(size_t i=0;i<x.size();++i){
        if(std::abs(x[i].x-y[i].x)>0.0001f || std::abs(x[i].y-y[i].y)>0.0001f ||
           std::abs(x[i].width-y[i].width)>0.0001f ||std::abs(x[i].height-y[i].height)>0.0001f){
            std::fprintf(stderr,"Geometry mismatch at %zu\n",i);return false;
        }
    }
    return true;
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    try {
        auto incremental=Document::load(argv[1]);
        auto baseline=Document::load(argv[1]);
        incremental.resize(960,540);
        auto initial=incremental.layout_stats();
        incremental.resize(960,540);
        if(incremental.layout_stats().layout_visits!=0){std::fprintf(stderr,"unchanged viewport recomputed\n");return 3;}
        if(incremental.layout_stats().painted_commands!=0){std::fprintf(stderr,"unchanged viewport repainted\n");return 4;}
        baseline.resize(960,540);
        if(!same(incremental,baseline))return 5;
        // A change in one leaf must invalidate its ancestors and affect any
        // flex/grid siblings that need reflow, but skip unrelated descendants.
        if(incremental.node_count()<5)return 6;
        incremental.set_size(4,220,-1);
        baseline.set_size(4,220,-1);
        incremental.resize(960,540);
        // baseline's first layout after a new load is the full reference.
        auto fresh=Document::load(argv[1]);
        fresh.set_size(4,220,-1);
        fresh.resize(960,540);
        if(!same(incremental,fresh))return 7;
        const auto partial=incremental.layout_stats();
        if(partial.layout_visits>=initial.layout_visits){
            std::fprintf(stderr,"incremental: %u, full: %u\n",partial.layout_visits,initial.layout_visits);return 8;
        }
        incremental.set_background(4,0xff123456u);
        incremental.resize(960,540);
        if(incremental.layout_stats().layout_visits!=0)return 9;
        if(!incremental.layout_stats().painted_commands)return 10;
        incremental.resize(960,540);
        if(incremental.layout_stats().painted_commands)return 11;
        std::printf("PASS initial=%u partial=%u skipped=%u\n",initial.layout_visits,partial.layout_visits,partial.layout_skips);
        return 0;
    } catch(std::exception const& e){std::fprintf(stderr,"%s\n",e.what());return 20;}
}
