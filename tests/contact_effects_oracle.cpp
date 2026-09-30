#include "sgn/contact_effects.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <type_traits>

namespace {
using namespace sgn::retail;
using namespace sgn::retail::contact;
using Record = std::array<std::uint8_t,0xa000>;
std::uint32_t u(const Record& b, unsigned at) { std::uint32_t v; std::memcpy(&v,b.data()+at,4); return v; }
void u(Record& b,unsigned at,std::uint32_t v) { std::memcpy(b.data()+at,&v,4); }
float f(const Record& b,unsigned at) { return std::bit_cast<float>(u(b,at)); }
void apply(Record& b,unsigned entry) {
    std::array<effects::PairVehicle,6> cars{};
    std::array<ContactBody,6> bodies{};
    std::array<Vector4,6> poses{};
    auto transfer = [&](bool reading) {
        auto field = [&](float& value,unsigned at) { if(reading)value=f(b,at);else u(b,at,std::bit_cast<std::uint32_t>(value)); };
        auto word = [&](std::uint32_t& value,unsigned at) { if(reading)value=u(b,at);else u(b,at,value); };
        auto integer = [&](std::int32_t& value,unsigned at) { if(reading)value=std::bit_cast<std::int32_t>(u(b,at));else u(b,at,std::bit_cast<std::uint32_t>(value)); };
        auto vector = [&](Vector4& value,unsigned at) { field(value.x,at);field(value.y,at+4);field(value.z,at+8);word(value.opaque,at+12); };
        for(unsigned i=0;i<6;++i) {
            const auto at=0x100+i*0x100;
            integer(cars[i].active,at); word(cars[i].cooldown,at+4); vector(cars[i].previous_position,at+8);
            vector(bodies[i].position,at+0x20);vector(bodies[i].velocity,at+0x30);vector(poses[i],at+0x40);field(bodies[i].mass,at+0x50);
            for(unsigned j=0;j<6;++j) { word(cars[i].counters.current[j],at+0x60+j*4);word(cars[i].counters.previous[j],at+0x78+j*4);word(cars[i].counters.count[j],at+0x90+j*4); }
        }
    };
    transfer(true);
    const auto current=u(b,8),second=u(b,12);
    if(current>=6||second>=6)throw std::runtime_error("bad vehicle fixture index");
    switch(entry) {
    case 0x421b80: {
        std::optional<unsigned> selected;if(u(b,0)!=0xffffffff)selected=u(b,0);
        const bool hit=effects::find_contact_partner(cars,current,std::bit_cast<std::int32_t>(u(b,4)),selected);
        u(b,0,selected.value_or(0xffffffff));u(b,16,hit);break;
    }
    case 0x41d270: effects::mark_pair_contact(cars,current,second);break;
    case 0x41d2c0: effects::update_pair_counters(cars[current].counters);break;
    case 0x4098a0: {
        const auto result=effects::apply_pair_impulse(bodies[current],bodies[second],poses[current],poses[second]);
        u(b,0x20,std::bit_cast<std::uint32_t>(result.direction.x));u(b,0x24,std::bit_cast<std::uint32_t>(result.direction.y));
        u(b,0x28,std::bit_cast<std::uint32_t>(result.direction.z));u(b,0x2c,std::bit_cast<std::uint32_t>(result.distance));
        u(b,0x30,std::bit_cast<std::uint32_t>(result.normal_speed));break;
    }
    case 0x415ac0: {
        effects::ImpactParticles state{};VehicleRandom random{u(b,0x90)};
        state.emitted=u(b,0x80);state.cursor=u(b,0x84);state.rate_selector=u(b,0x88);state.throttle=std::bit_cast<std::int32_t>(u(b,0x8c));
        static_assert(sizeof(effects::ImpactParticle)==0x440 && std::is_trivially_copyable_v<effects::ImpactParticle>);
        std::memcpy(state.particles.data(),b.data()+0x1000,0x8800);
        effects::emit_impact(state,random,{f(b,0x40),f(b,0x44),f(b,0x48),u(b,0x4c)},
            {f(b,0x50),f(b,0x54),f(b,0x58),u(b,0x5c)},f(b,0x60));
        std::memcpy(b.data()+0x1000,state.particles.data(),0x8800);
        u(b,0x80,state.emitted);u(b,0x84,state.cursor);u(b,0x8c,std::bit_cast<std::uint32_t>(state.throttle));u(b,0x90,random.state);break;
    }
    default:throw std::runtime_error("bad effect entry");
    }
    transfer(false);
}
void read(std::ifstream& in,void* data,std::size_t size) { if(!in.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw std::runtime_error("short effect corpus"); }
std::uint32_t read_u(std::ifstream& in) {std::uint32_t v;read(in,&v,4);return v;}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected corpus path");
        std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNCE001")throw std::runtime_error("effect corpus header");
        const auto count=read_u(in);if(count>10000)throw std::runtime_error("effect corpus count");
        unsigned errors=0;std::uint64_t exact=0;
        for(unsigned n=0;n<count;++n) {
            const auto entry=read_u(in);Record actual{},expected{};read(in,actual.data(),actual.size());read(in,expected.data(),expected.size());apply(actual,entry);
            for(unsigned at=0;at<actual.size();at+=4) {
                if(u(actual,at)==u(expected,at)) { ++exact;continue; }
                if(errors++<24)std::cerr<<"case "<<n<<" entry "<<std::hex<<entry<<" offset "<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing effect corpus");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0}\n";
        return errors?1:0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
