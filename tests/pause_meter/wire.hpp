#pragma once
#include "../text_submission_wire.hpp"
#include <span>
#include <string_view>
namespace pause_meter_wire {
using text_wire::font_size;using text_wire::fonts;using text_wire::index;using text_wire::map;
using text_wire::globals;using text_wire::viewport;using text_wire::materials;using text_wire::quad;using text_wire::alpha;
using text_wire::require;using text_wire::read;using text_wire::number;using text_wire::word;
inline constexpr unsigned clip=alpha+320,labels=clip+36,total=labels+8;
using StateBytes=std::array<std::uint8_t,total>;
inline std::uint32_t word(const StateBytes& b,std::size_t at){return text_wire::word(b.data()+at);}
inline void put(StateBytes& b,std::size_t at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
inline constexpr std::array<std::array<std::uint8_t,8>,6> bank={{{'-',0},{'+',0},{'A','B',0},{0},{'A','\n',0},{'0','1','2','3','4','5',0}}};
inline std::span<const std::uint8_t> label(std::uint32_t token){require(token<bank.size(),"meter label token bound");return bank[token];}
inline constexpr std::array<unsigned,9> clip_addresses={0x592018,0x59201c,0x592020,0x592030,0x592034,0x592038,0x592040,0x592044,0x592048};
struct Patch{unsigned at{},value{};};struct Effect{unsigned event{},at{},value{};};
struct Step{unsigned percent{},hresult{};std::array<unsigned,4> args{};std::vector<Patch> patches;std::vector<Effect> effects;};
inline bool patchable(unsigned at){return at%4==0&&at<total;}
inline Step read_step(std::ifstream& in){Step s;s.percent=number(in);read(in,s.args.data(),16);s.hresult=number(in);
    auto n=number(in);require(n<=1024,"meter patch bound");for(unsigned i=0;i<n;++i){Patch p{number(in),number(in)};require(patchable(p.at),"meter patch address");s.patches.push_back(p);}
    n=number(in);require(n<=32,"meter effect bound");for(unsigned i=0;i<n;++i){Effect e{number(in),number(in),number(in)};require(e.event<4096&&patchable(e.at),"meter effect bounds");s.effects.push_back(e);}return s;}
inline void write(std::ofstream& out,const void* p,std::size_t n){require(bool(out.write(static_cast<const char*>(p),static_cast<std::streamsize>(n))),"meter output write");}
inline void number(std::ofstream& out,unsigned value){write(out,&value,4);}
inline void write_step(std::ofstream& out,const Step& s){number(out,s.percent);write(out,s.args.data(),16);number(out,s.hresult);number(out,static_cast<unsigned>(s.patches.size()));for(auto p:s.patches){number(out,p.at);number(out,p.value);}number(out,static_cast<unsigned>(s.effects.size()));for(auto e:s.effects){number(out,e.event);number(out,e.at);number(out,e.value);}}
inline bool local_uv1(const std::vector<unsigned>& e,unsigned w){return e[0]==4&&w>=7&&(w-7)%10>=8;}
inline void validate(const std::vector<unsigned>& e){require(!e.empty(),"meter empty event");const auto k=e[0];require(k>=1&&k<=5,"meter event opcode");
    if(k<=3){require(e.size()==(k==3?6u:5u),"meter state event shape");return;}
    require(e.size()>=7&&e[4]==0x2c4&&e[6]==0x14,"meter draw FVF/flags");
    require((e[3]==6&&e[5]==4)||(k==5&&e[3]==2&&e[5]==2),"meter draw primitive/extent");require(e.size()==7+10*e[5],"meter draw payload extent");}
}
