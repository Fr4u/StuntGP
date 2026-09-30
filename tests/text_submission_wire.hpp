#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace text_wire {
inline constexpr std::uint32_t font_size=0x1ba0,fonts=0,index=2*font_size,map=index+160,
    globals=map+256,viewport=globals+28,materials=viewport+152,quad=materials+255*28,alpha=quad+140,total=alpha+160;
using StateBytes=std::array<std::uint8_t,total>;
inline void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
inline std::uint32_t word(const void* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
inline std::uint32_t word(const StateBytes& b,std::size_t at){return word(b.data()+at);}
inline void put(StateBytes& b,std::size_t at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
inline void read(std::ifstream& in,void* p,std::size_t n){require(bool(in.read(static_cast<char*>(p),static_cast<std::streamsize>(n))),"truncated text corpus");}
inline std::uint32_t number(std::ifstream& in){std::uint32_t v;read(in,&v,4);return v;}
inline bool patchable(std::uint32_t at){return at%4==0&&at<total;}
struct Patch {std::uint32_t at{},value{};};
struct Effect {std::uint32_t api{},at{},value{};};
struct Step {
    //op0 full4291F0,1 measure4290D0,2 begin432300,3 glyph4323F0,4 end432380.
    std::uint32_t op{},selector{},character{};
    std::array<std::uint32_t,6> parameters{}; //x,y,z,color,scale,flags
    std::vector<std::uint8_t> text;
    std::vector<Patch> patches;
    std::vector<Effect> effects;
    std::uint32_t hresult{};
};
inline Step read_step(std::ifstream& in){
    Step s;s.op=number(in);s.selector=number(in);s.character=number(in);require(s.op<=4&&s.character<=255,"text operation domain");
    read(in,s.parameters.data(),24);s.hresult=number(in);const auto n=number(in);require(n<=4096,"text byte bound");s.text.resize(n);read(in,s.text.data(),n);
    const auto p=number(in);require(p<=1024,"text patch bound");
    for(unsigned i=0;i<p;++i){Patch item{number(in),number(in)};require(patchable(item.at),"text patch outside state");s.patches.push_back(item);}
    const auto e=number(in);require(e<=32,"text callback bound");
    for(unsigned i=0;i<e;++i){Effect item{number(in),number(in),number(in)};require(item.api<4096&&patchable(item.at),"text callback bounds");s.effects.push_back(item);}
    return s;
}
inline bool opaque_draw_word(std::uint32_t op,std::size_t payload_word){return op==4&&payload_word>=4&&((payload_word-4)%10)>=8;}
}
