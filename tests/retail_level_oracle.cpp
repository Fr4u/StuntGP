#include "sgn/retail_level.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
using namespace sgn::retail;
namespace {
using Bytes=std::vector<std::uint8_t>;
constexpr std::uint32_t base=0x60df98,size=0x1320;
struct Reader {
    std::span<const std::uint8_t> data;std::size_t at{};
    std::span<const std::uint8_t> bytes(std::size_t n){if(at>data.size()||n>data.size()-at)throw std::runtime_error("truncated level corpus");const auto out=data.subspan(at,n);at+=n;return out;}
    std::uint32_t word(){const auto b=bytes(4);return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
    std::string text(std::size_t n){const auto b=bytes(n);return {b.begin(),b.end()};}
};
void append(Bytes& b,std::uint32_t value){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
std::uint32_t get(const Bytes& b,std::uint32_t address){Reader r{b};r.at=address-base;return r.word();}
void put(Bytes& b,std::uint32_t address,std::uint32_t value){for(unsigned i=0;i<4;++i)b.at(address-base+i)=static_cast<std::uint8_t>(value>>(i*8));}
LevelSettings decode(const Bytes& b){LevelSettings s{};
    s.game_type=get(b,0x60df98);s.music_track=get(b,0x60dfa4);s.ground_level=std::bit_cast<float>(get(b,0x60dfa8));
    std::memcpy(s.track_texture.data(),b.data()+0x60dfac-base,64);s.texture_suffix=get(b,0x60dfec);
    s.time_ticks=get(b,0x60eff4);s.lap_count=get(b,0x60eff8);s.lap_record_ticks=get(b,0x60f000);
    s.rolling_start_offset=get(b,0x60f014);s.camera_mode=get(b,0x60f01c);s.overlay_demo=get(b,0x60f024);s.demo_loop_ticks=get(b,0x60f028);return s;}
void encode(Bytes& b,const LevelSettings& s){
    put(b,0x60df98,s.game_type);put(b,0x60dfa4,s.music_track);put(b,0x60dfa8,std::bit_cast<std::uint32_t>(s.ground_level));
    std::memcpy(b.data()+0x60dfac-base,s.track_texture.data(),64);put(b,0x60dfec,s.texture_suffix);
    put(b,0x60eff4,s.time_ticks);put(b,0x60eff8,s.lap_count);put(b,0x60f000,s.lap_record_ticks);
    put(b,0x60f014,s.rolling_start_offset);put(b,0x60f01c,s.camera_mode);put(b,0x60f024,s.overlay_demo);put(b,0x60f028,s.demo_loop_ticks);
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("usage: retail_level_oracle corpus");std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("cannot open level corpus");const auto length=file.tellg();if(length<0||length>128*1024*1024)throw std::runtime_error("excessive corpus");
    Bytes raw(static_cast<std::size_t>(length));file.seekg(0);file.read(reinterpret_cast<char*>(raw.data()),length);if(!file)throw std::runtime_error("short corpus read");
    Reader r{raw};if(r.text(8)!="SGNLV001")throw std::runtime_error("invalid level corpus signature");const auto count=r.word();if(!count||count>100000)throw std::runtime_error("invalid level case count");std::uint64_t compared=0;
    for(std::uint32_t c=0;c<count;++c){const auto op=r.word(),text_length=r.word(),end=r.word(),label_length=r.word(),expected_length=r.word();
        if(op>4||text_length>131072||label_length>128||expected_length>65536)throw std::runtime_error("invalid level case shape");
        const auto text=r.text(text_length),label=r.text(label_length);const auto seed=r.bytes(size),expected=r.bytes(expected_length);Bytes out;
        if(op<2){out.assign(seed.begin(),seed.end());if(op==0){auto state=decode(out);apply_level_settings({text,end},state);encode(out,state);}else{
            const auto cars=parse_level_car_settings({text,end});for(std::size_t i=0;i<cars.size();++i){const auto& v=cars[i];const std::array<std::uint32_t,6> values={v.config,v.tyre,v.livery,v.replay,v.ai_skill,v.ai_mode};for(unsigned j=0;j<6;++j)put(out,0x60f038+static_cast<std::uint32_t>(i)*100+j*4,values[j]);}}}
        else if(op==2){const auto assets=parse_level_asset_bindings({text,end},label=="cars");append(out,static_cast<std::uint32_t>(assets.requests.size()));
            for(const auto& a:assets.requests){append(out,a.type);append(out,static_cast<std::uint32_t>(a.name.size()));out.insert(out.end(),a.name.begin(),a.name.end());}}
        else if(op==3){const auto b=find_level_block(text,label);if(!b)throw std::runtime_error("expected block missing");append(out,static_cast<std::uint32_t>(b->text.data()-text.data()));append(out,static_cast<std::uint32_t>(b->end));}
        else {const auto names=discover_level_blocks(text);append(out,static_cast<std::uint32_t>(names.size()));for(const auto& name:names){append(out,static_cast<std::uint32_t>(name.size()));out.insert(out.end(),name.begin(),name.end());}}
        if(out.size()!=expected.size()){std::cerr<<"case "<<c<<" op "<<op<<" output size mismatch "<<out.size()<<" vs "<<expected.size()<<'\n';return 1;}
        for(std::size_t i=0;i<out.size();++i){++compared;if(out[i]!=expected[i]){std::cerr<<"case "<<c<<" op "<<op<<" byte "<<i<<" native "<<unsigned(out[i])<<" original "<<unsigned(expected[i])<<'\n';return 1;}}
    }
    if(r.at!=raw.size())throw std::runtime_error("trailing level corpus bytes");std::cout<<count<<" cases; "<<compared<<" bytes compared; 0 mismatches; tolerance 0 ULP\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
