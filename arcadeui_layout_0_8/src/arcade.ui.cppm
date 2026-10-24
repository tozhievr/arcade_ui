module;
#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <memory_resource>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
export module arcade.ui;

export namespace arcade::ui {
    struct Rect { float x{}, y{}, width{}, height{}; };
    struct Event { std::uint32_t target{}; std::string_view name; };
    struct DrawCommand { Rect rect; std::uint32_t rgba{}; std::string_view text; std::uint32_t text_rgba{}; float font_size{}; };
    using Handler = void (*)(void*, const Event&) noexcept;
    struct Callback { Handler function{}; void* context{}; };
    struct Node { std::uint32_t parent{}, first_child{}, child_count{}, style{}, text_offset{}, text_size{}, event_offset{}, event_size{}; };
    struct Style {
        std::uint32_t display{}, direction{};
        float width{}, height{}, grow{}, shrink{}, gap{}, padding{}, margin{}, font_size{};
        std::uint32_t background{}, foreground{}, flags{}, justify{}, align{}, wrap{}, grid_columns{};
        float basis{-1}, min_width{-1}, max_width{-1};
        std::uint32_t track_offset{}, grid_span{1}, track_count{};
    };
    static_assert(sizeof(Node)==32);
    static_assert(sizeof(Style)==92);
    struct Track { std::uint32_t kind{}; float value{}; };
    static_assert(sizeof(Track)==8);

    struct LayoutStats {
        std::uint32_t layout_visits{};
        std::uint32_t layout_skips{};
        std::uint32_t measure_computations{};
        std::uint32_t measure_cache_hits{};
        std::uint32_t painted_commands{};
    };

    class Document {
        std::pmr::vector<std::byte> bytes_;
        std::pmr::vector<Node> nodes_;
        std::pmr::vector<Style> styles_;
        std::pmr::vector<Track> tracks_;
        std::pmr::vector<Rect> rectangles_;
        std::pmr::vector<DrawCommand> commands_;
        std::pmr::unordered_map<std::string, Callback> callbacks_;
        // Dirtiness is a compact layout dependency graph: parent indices encode
        // upward intrinsic-size dependencies, while contiguous child ranges
        // preserve the source-order layout dependency graph.
        // bit 0: subtree needs arranging; bit 1: node's own style changed.
        std::pmr::vector<std::uint8_t> dirty_;
        std::pmr::vector<float> measured_width_;
        std::pmr::vector<float> measured_height_;
        std::pmr::vector<std::uint8_t> measure_valid_;
        LayoutStats stats_{};
        bool laid_out_{};
        bool paint_dirty_{true};
        std::uint32_t strings_start_{}, strings_size_{};
        float viewport_width_{}, viewport_height_{};
        [[nodiscard]] static float clamp_width(float w,const Style& s) noexcept {
            if(s.min_width>=0)w=std::max(w,s.min_width);
            if(s.max_width>=0)w=std::min(w,s.max_width);
            return std::max(0.f,w);
        }
        [[nodiscard]] float flex_basis(std::uint32_t id,bool horizontal) {
            const auto& s=styles_[nodes_[id].style];
            return horizontal?clamp_width(s.basis>=0?s.basis:measure(id,true),s):
                (s.basis>=0?s.basis:measure(id,false));
        }
        static std::uint32_t u32(const std::byte* p) noexcept {
            return std::uint32_t(std::to_integer<unsigned char>(p[0])) |
                (std::uint32_t(std::to_integer<unsigned char>(p[1])) << 8) |
                (std::uint32_t(std::to_integer<unsigned char>(p[2])) << 16) |
                (std::uint32_t(std::to_integer<unsigned char>(p[3])) << 24);
        }
        static float f32(const std::byte* p) noexcept { auto v=u32(p); return std::bit_cast<float>(v); }
        [[nodiscard]] std::string_view text(std::uint32_t off,std::uint32_t len) const {
            if (off>strings_size_ || len>strings_size_-off) throw std::runtime_error("Invalid string offset");
            return {reinterpret_cast<const char*>(bytes_.data()+strings_start_+off),len};
        }
        [[nodiscard]] float measure(std::uint32_t id,bool horizontal) {
            const auto bit=horizontal?std::uint8_t{1}:std::uint8_t{2};
            if(measure_valid_[id]&bit){++stats_.measure_cache_hits;return horizontal?measured_width_[id]:measured_height_[id];}
            ++stats_.measure_computations;
            const auto& n=nodes_[id];const auto& s=styles_[n.style];
            if (s.display==3) { measure_valid_[id]|=bit; return 0; }
            const float fixed=horizontal?s.width:s.height;
            if (fixed>=0) {
                (horizontal?measured_width_[id]:measured_height_[id])=fixed;
                measure_valid_[id]|=bit;
                return horizontal?clamp_width(fixed,s):fixed;
            }
            const float pad=s.padding*2;
            if (!n.child_count){
                const auto value=pad+(horizontal?float(n.text_size)*s.font_size*0.58f:s.font_size*1.3f);
                (horizontal?measured_width_[id]:measured_height_[id])=value;
                measure_valid_[id]|=bit;
                return horizontal?clamp_width(value,s):value;
            }
            float result=0;
            const bool main=(s.display==1 && ((s.direction==0)==horizontal));
            if (main) {
                for(std::uint32_t i=0;i<n.child_count;++i)result+=measure(n.first_child+i,horizontal)+styles_[nodes_[n.first_child+i].style].margin*2;
                result+=s.gap*float(n.child_count-1);
            } else {
                for(std::uint32_t i=0;i<n.child_count;++i)result=std::max(result,measure(n.first_child+i,horizontal)+styles_[nodes_[n.first_child+i].style].margin*2);
            }
            result+=pad;
            (horizontal?measured_width_[id]:measured_height_[id])=result;
            measure_valid_[id]|=bit;
            return horizontal?clamp_width(result,s):result;
        }
        void layout(std::uint32_t id,Rect area) {
            const auto previous=rectangles_[id];
            const bool changed=!laid_out_ || previous.x!=area.x || previous.y!=area.y ||
                previous.width!=area.width || previous.height!=area.height;
            if(!changed && !dirty_[id]) {++stats_.layout_skips;return;}
            ++stats_.layout_visits;
            rectangles_[id]=area;
            // Reset this node before descendants; propagate a geometry change
            // to the children even when their local styles are unchanged.
            dirty_[id]=0;
            const auto& n=nodes_[id];const auto& s=styles_[n.style];
            if(s.display==3 || !n.child_count)return;
            const float pad=s.padding;
            const float innerw=std::max(0.f,area.width-2*pad),innerh=std::max(0.f,area.height-2*pad);
            if (s.display==2) {
                const auto columns=std::max(1u,s.grid_columns);
                std::vector<float> widths(columns,0.f);
                float available=std::max(0.f,innerw-s.gap*float(columns-1));
                std::vector<float> weights(columns,0.f);
                std::vector<std::uint8_t> flexible(columns,0);
                float weight_sum=0.f;
                for(std::uint32_t col=0;col<columns;++col){
                    const auto& t=tracks_[s.track_offset+col];
                    if(t.kind==0){widths[col]=t.value;available-=t.value;}
                    else {
                        widths[col]=t.kind==2?t.value:0.f;
                        weights[col]=t.kind==1?t.value:1.f;
                        flexible[col]=1;
                        weight_sum+=weights[col];
                    }
                }
                // Resolve fractional tracks with minmax() lower bounds.
                // Freeze tracks whose minimum exceeds the current fraction unit.
                for(std::uint32_t iteration=0;iteration<columns && weight_sum>0.f;++iteration){
                    const float unit=std::max(0.f,available)/weight_sum;
                    bool frozen=false;
                    for(std::uint32_t col=0;col<columns;++col){
                        if(flexible[col] && widths[col]>unit*weights[col]){
                            available-=widths[col];weight_sum-=weights[col];
                            flexible[col]=0;frozen=true;
                        }
                    }
                    if(!frozen)break;
                }
                const float unit=weight_sum>0.f?std::max(0.f,available)/weight_sum:0.f;
                for(std::uint32_t col=0;col<columns;++col){
                    if(flexible[col])widths[col]=unit*weights[col];
                }
                std::vector<float> starts(columns);
                float xx=area.x+pad;
                for(std::uint32_t col=0;col<columns;++col){starts[col]=xx;xx+=widths[col]+s.gap;}
                // AOT nodes are in source order; implicit auto-placement flows across rows.
                // Tracks spanning several columns occupy their combined width.
                struct Placement {std::uint32_t child,row,col,span;};
                std::vector<Placement> placements;
                std::uint32_t row=0,col=0;
                for(std::uint32_t j=0;j<n.child_count;++j){
                    const auto child=n.first_child+j;
                    const auto& cs=styles_[nodes_[child].style];
                    if(cs.display==3){layout(child,{});continue;}
                    const auto span=std::min(columns,std::max(1u,cs.grid_span));
                    if(col+span>columns){++row;col=0;}
                    placements.push_back({child,row,col,span});
                    col+=span;if(col>=columns){++row;col=0;}
                }
                std::uint32_t row_count=0;
                for(const auto& p:placements)row_count=std::max(row_count,p.row+1);
                std::vector<float> heights(row_count,0.f),ys(row_count,area.y+pad);
                for(const auto& p:placements){
                    const auto& cs=styles_[nodes_[p.child].style];
                    heights[p.row]=std::max(heights[p.row],measure(p.child,false)+2*cs.margin);
                }
                for(std::uint32_t r=1;r<row_count;++r)ys[r]=ys[r-1]+heights[r-1]+s.gap;
                for(const auto& p:placements){
                    const auto& cs=styles_[nodes_[p.child].style];
                    float width=0.f;
                    for(std::uint32_t c=p.col;c<p.col+p.span;++c)width+=widths[c];
                    width+=s.gap*float(p.span-1);
                    const auto available_width=std::max(0.f,width-2*cs.margin);
                    const auto child_width=clamp_width(cs.width>=0?cs.width:available_width,cs);
                    const auto child_height=cs.height>=0?cs.height:std::max(0.f,heights[p.row]-2*cs.margin);
                    layout(p.child,{starts[p.col]+cs.margin,ys[p.row]+cs.margin,child_width,child_height});
                }
                return;
            }
            const bool horizontal=(s.display==1 && s.direction==0);
            const float available=horizontal?innerw:innerh;
            // A flex line is computed as a contiguous interval, preserving source order.
            std::uint32_t begin=0;
            float cross_cursor=0.f;
            while(begin<n.child_count){
                std::uint32_t end=begin;
                float basis=0.f,grow_total=0.f,shrink_weight=0.f,line_cross=0.f;
                while(end<n.child_count){
                    const auto child=n.first_child+end;
                    const auto& cs=styles_[nodes_[child].style];
                    if(cs.display==3){++end;continue;}
                    const float b=flex_basis(child,horizontal);
                    const float outer=b+cs.margin*2;
                    const float needed=outer+(end>begin?s.gap:0.f);
                    if(s.wrap && end>begin && basis+needed>available)break;
                    basis+=needed;grow_total+=std::max(0.f,cs.grow);
                    shrink_weight+=b*std::max(0.f,cs.shrink);
                    line_cross=std::max(line_cross,measure(child,!horizontal)+2*cs.margin);
                    ++end;
                }
                if(end==begin)++end;
                const float free=available-basis;
                float offset=0.f,extra_gap=0.f;
                if(free>0.f && grow_total==0.f){
                    if(s.justify==1)offset=free/2;
                    else if(s.justify==2)offset=free;
                    else if(s.justify==3 && end-begin>1)extra_gap=free/float(end-begin-1);
                }
                float cursor=(horizontal?area.x:area.y)+pad+offset;
                const float cross_space=s.wrap?line_cross:(horizontal?innerh:innerw);
                for(std::uint32_t i=begin;i<end;++i){
                    const auto child=n.first_child+i;
                    const auto& cs=styles_[nodes_[child].style];
                    if(cs.display==3){layout(child,{});continue;}
                    const float b=flex_basis(child,horizontal);
                    float main=b;
                    if(free>0 && grow_total>0)main+=free*std::max(0.f,cs.grow)/grow_total;
                    if(free<0 && shrink_weight>0)main=std::max(0.f,b+free*b*std::max(0.f,cs.shrink)/shrink_weight);
                    if(horizontal)main=clamp_width(main,cs);
                    const float preferred_cross=measure(child,!horizontal);
                    const float available_cross=std::max(0.f,cross_space-2*cs.margin);
                    const float cross=s.align==0?available_cross:std::min(available_cross,preferred_cross);
                    float cross_offset=0.f;
                    if(s.align==2)cross_offset=(available_cross-cross)/2;
                    if(s.align==3)cross_offset=available_cross-cross;
                    const float cross_origin=(horizontal?area.y:area.x)+pad+cross_cursor+cs.margin+cross_offset;
                    Rect r=horizontal?Rect{cursor+cs.margin,cross_origin,main,cross}:
                        Rect{cross_origin,cursor+cs.margin,cross,main};
                    layout(child,r);
                    cursor+=main+2*cs.margin+s.gap+extra_gap;
                }
                cross_cursor+=line_cross+s.gap;
                begin=end;
            }
        }
        void render() {
            if(!paint_dirty_)return;
            commands_.clear();
            for(std::size_t i=0;i<nodes_.size();++i){auto& n=nodes_[i];auto& s=styles_[n.style];if(s.display==3)continue;
                bool hidden=false;auto ancestor=n.parent;while(ancestor!=0xffffffff){if(styles_[nodes_[ancestor].style].display==3){hidden=true;break;}ancestor=nodes_[ancestor].parent;}if(hidden)continue;
                commands_.push_back({rectangles_[i],s.background,text(n.text_offset,n.text_size),s.foreground,s.font_size});
            }
            stats_.painted_commands=static_cast<std::uint32_t>(commands_.size());
            paint_dirty_=false;
        }
        void mark_layout_dirty(std::uint32_t id) {
            if(id>=nodes_.size())throw std::out_of_range("Invalid node ID");
            // A change in one child can affect all its flex/grid siblings,
            // but only its ancestors need marking: arrange recomputes
            // sibling positions and unchanged children are skipped.
            while(id!=0xffffffffu){
                dirty_[id]|=1;
                measure_valid_[id]=0;
                id=nodes_[id].parent;
            }
            paint_dirty_=true;
        }
    public:
        explicit Document(std::pmr::memory_resource* memory=std::pmr::get_default_resource()):bytes_(memory),nodes_(memory),styles_(memory),tracks_(memory),rectangles_(memory),commands_(memory),callbacks_(memory),dirty_(memory),measured_width_(memory),measured_height_(memory),measure_valid_(memory){}
        [[nodiscard]] static Document load(const std::string& path,std::pmr::memory_resource* memory=std::pmr::get_default_resource()){
            Document result(memory);std::ifstream stream(path,std::ios::binary|std::ios::ate);
            if(!stream)throw std::runtime_error("Cannot open arcuxi");auto end=stream.tellg();if(end<72 || end>std::streamoff(256*1024*1024))throw std::runtime_error("Invalid image size");
            result.bytes_.resize(static_cast<std::size_t>(end));stream.seekg(0);stream.read(reinterpret_cast<char*>(result.bytes_.data()),end);
            if(!stream)throw std::runtime_error("Truncated image");const auto* b=result.bytes_.data();
            const std::array<char,8> magic={'A','R','C','U','X','I','\0','\x01'};
            if(std::memcmp(b,magic.data(),8)!=0)throw std::runtime_error("Invalid magic");
            auto version=u32(b+8), count=u32(b+12), nbytes=u32(b+16),sbytes=u32(b+20),strbytes=u32(b+24),abytes=u32(b+28), tbytes=u32(b+32);
            if(version!=3||count>1000000||nbytes!=count*sizeof(Node)||sbytes!=count*sizeof(Style)||tbytes%sizeof(Track)||tbytes>4096u*8u*count)throw std::runtime_error("Invalid header");
            std::uint64_t total=72ull+nbytes+sbytes+tbytes+strbytes+abytes;
            if(total!=result.bytes_.size())throw std::runtime_error("Invalid sections");
            result.strings_start_=72+nbytes+sbytes+tbytes;result.strings_size_=strbytes;
            for(std::uint32_t i=0;i<count;++i){const auto* x=b+72+std::size_t(i)*32;Node n{};
                n.parent=u32(x);n.first_child=u32(x+4);n.child_count=u32(x+8);n.style=u32(x+12);n.text_offset=u32(x+16);n.text_size=u32(x+20);n.event_offset=u32(x+24);n.event_size=u32(x+28);
                if(n.style>=count||(n.parent!=0xffffffff && n.parent>=i)||(n.child_count && (n.first_child<=i || std::uint64_t(n.first_child)+n.child_count>count)))throw std::runtime_error("Invalid topology");
                result.nodes_.push_back(n);(void)result.text(n.text_offset,n.text_size);(void)result.text(n.event_offset,n.event_size);
            }
            for(std::uint32_t i=0;i<count;++i){auto x=b+72+nbytes+std::size_t(i)*92;Style s{};
                s.display=u32(x);s.direction=u32(x+4);s.width=f32(x+8);s.height=f32(x+12);s.grow=f32(x+16);s.shrink=f32(x+20);s.gap=f32(x+24);s.padding=f32(x+28);s.margin=f32(x+32);s.font_size=f32(x+36);s.background=u32(x+40);s.foreground=u32(x+44);s.flags=u32(x+48);s.justify=u32(x+52);s.align=u32(x+56);s.wrap=u32(x+60);s.grid_columns=u32(x+64);s.basis=f32(x+68);s.min_width=f32(x+72);s.max_width=f32(x+76);s.track_offset=u32(x+80);s.grid_span=u32(x+84);s.track_count=u32(x+88);
                if(s.display>3||s.direction>1||s.justify>3||s.align>3||s.wrap>1||s.grid_columns>4096||s.grid_span<1||s.grid_span>4096||s.track_count!=s.grid_columns||std::uint64_t(s.track_offset)+s.track_count>tbytes/8||!std::isfinite(s.basis)||!std::isfinite(s.min_width)||!std::isfinite(s.max_width))throw std::runtime_error("Invalid style");result.styles_.push_back(s);
            }
            for(std::uint32_t i=0;i<tbytes/8;++i){
                const auto* t=b+72+nbytes+sbytes+std::size_t(i)*8;Track v{u32(t),f32(t+4)};
                if(v.kind>2||!std::isfinite(v.value)||v.value<0)throw std::runtime_error("Invalid track");
                result.tracks_.push_back(v);
            }
            result.rectangles_.resize(count);
            result.dirty_.assign(count,1);
            result.measured_width_.assign(count,0.f);
            result.measured_height_.assign(count,0.f);
            result.measure_valid_.assign(count,0);
            return result;
        }
        void resize(float w,float h){
            if(!std::isfinite(w)||!std::isfinite(h)||w<0||h<0)throw std::invalid_argument("Invalid viewport");
            stats_={};
            const bool viewport_changed=viewport_width_!=w || viewport_height_!=h;
            viewport_width_=w;viewport_height_=h;
            if(nodes_.empty())return;
            if(viewport_changed){
                // Viewport-related percentage rules will require more granular
                // tracking when the supported CSS profile is expanded.
                dirty_[0]|=1;
                paint_dirty_=true;
            }
            const auto& root=styles_[nodes_[0].style];
            layout(0,{0,0,root.width>=0?root.width:w,root.height>=0?root.height:h});
            laid_out_=true;
            render();
        }
        void invalidate(std::uint32_t id){mark_layout_dirty(id);}
        void set_size(std::uint32_t id,float width,float height){
            if(id>=nodes_.size())throw std::out_of_range("Invalid node ID");
            if((width<0 && width!=-1) || (height<0 && height!=-1) || !std::isfinite(width) || !std::isfinite(height))
                throw std::invalid_argument("Invalid dimensions");
            auto& style=styles_[nodes_[id].style];
            if(style.width==width && style.height==height)return;
            style.width=width;style.height=height;
            mark_layout_dirty(id);
        }
        void set_background(std::uint32_t id,std::uint32_t rgba){
            if(id>=nodes_.size())throw std::out_of_range("Invalid node ID");
            auto& style=styles_[nodes_[id].style];
            if(style.background==rgba)return;
            style.background=rgba;paint_dirty_=true;
        }
        [[nodiscard]] LayoutStats layout_stats() const noexcept{return stats_;}
        void write_svg(std::ostream& out) const {
            out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << viewport_width_ << ' ' << viewport_height_ << "\">\n";
            for(const auto& cmd:commands_) {
                const auto color=[&](std::uint32_t c){
                    constexpr char digits[]="0123456789abcdef";
                    char code[8]={'#', '#', '#', '#', '#', '#', '#', 0};
                    for(int j=0;j<6;++j)code[j+1]=digits[(c>>(20-4*j))&15];
                    return std::string(code);
                };
                if((cmd.rgba>>24)!=0){out << "<rect x=\""<<cmd.rect.x<<"\" y=\""<<cmd.rect.y<<"\" width=\""<<cmd.rect.width<<"\" height=\""<<cmd.rect.height<<"\" fill=\""<<color(cmd.rgba)<<"\" fill-opacity=\""<<float(cmd.rgba>>24)/255.f<<"\"/>\n";}
                if(!cmd.text.empty()){
                    out << "<text x=\""<<cmd.rect.x<<"\" y=\""<<cmd.rect.y+cmd.font_size<<"\" font-size=\""<<cmd.font_size<<"\" fill=\""<<color(cmd.text_rgba)<<"\">";
                    for(char ch:cmd.text){switch(ch){case '&':out<<"&amp;";break;case '<':out<<"&lt;";break;case '>':out<<"&gt;";break;case '\"':out<<"&quot;";break;default:out<<ch;}}
                    out << "</text>\n";
                }
            }
            out << "</svg>\n";
        }
        [[nodiscard]] std::span<const DrawCommand> commands() const noexcept{return commands_;}
        [[nodiscard]] std::span<const Rect> geometry() const noexcept{return rectangles_;}
        [[nodiscard]] std::size_t node_count() const noexcept{return nodes_.size();}
        void on(std::string name,Callback callback){callbacks_.insert_or_assign(std::move(name),callback);}
        bool dispatch(std::uint32_t node){if(node>=nodes_.size())return false;auto& n=nodes_[node];auto name=text(n.event_offset,n.event_size);if(name.empty())return false;auto it=callbacks_.find(std::string(name));if(it==callbacks_.end()||!it->second.function)return false;
            it->second.function(it->second.context,Event{node,name});return true;}
        bool click(float x,float y){for(std::size_t i=nodes_.size();i>0;--i){auto id=std::uint32_t(i-1);auto& r=rectangles_[id];if(x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height&&dispatch(id))return true;}return false;}
    };
}
