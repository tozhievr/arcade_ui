#include <cstdio>
#include <exception>
#include <fstream>
import arcade.ui;
int main(int argc,char** argv){
    if(argc<2){std::fprintf(stderr,"Usage: arcadeui <file.arcuxi>\n");return 2;}
    try{auto doc=arcade::ui::Document::load(argv[1]);doc.resize(960,540);
        std::printf("loaded %zu nodes, %zu draw commands\n",doc.node_count(),doc.commands().size());
        auto cmds=doc.commands();for(std::size_t i=0;i<cmds.size();++i){const auto& c=cmds[i];if(!c.text.empty())std::printf("text (%g,%g): %.*s\n",c.rect.x,c.rect.y,int(c.text.size()),c.text.data());}
        if(argc>2){std::ofstream svg(argv[2]);if(!svg)return 3;doc.write_svg(svg);}
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"arcadeui: %s\n",e.what());return 1;}
}
