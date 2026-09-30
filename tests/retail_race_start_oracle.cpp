#include "sgn/retail_race_start.hpp"
#include "contact_solver_state.hpp"
#include <fstream>
#include <iostream>
#include <vector>

namespace {
using namespace sgn::retail;
constexpr unsigned record=0x7800,tail=record*6,size=tail+1024;
using Bytes=std::vector<std::uint8_t>;
std::uint32_t get(const Bytes& b,unsigned at) {std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
void put(Bytes& b,unsigned at,std::uint32_t v) {std::memcpy(b.data()+at,&v,4);}
float real(const Bytes& b,unsigned at) {return std::bit_cast<float>(get(b,at));}
void event(Bytes& b,unsigned entry,unsigned a,unsigned c) {
    const auto n=get(b,tail+96);if(n>=16)throw std::runtime_error("race start trace overflow");
    put(b,tail+100+n*12,entry);put(b,tail+104+n*12,a);put(b,tail+108+n*12,c);put(b,tail+96,n+1);
}
struct Presentation:RaceStartPresentation {
    Bytes& bytes;
    explicit Presentation(Bytes& b):bytes(b){}
    void sound(unsigned id,unsigned volume) override {event(bytes,0x461150,id,volume);}
    unsigned selected_texture() const override {return get(bytes,tail+16);}
    void texture(unsigned slot,unsigned id) override {
        event(bytes,0x430d40,slot,id);
        if(get(bytes,tail+300))put(bytes,tail+16,id);
    }
};
void apply(Bytes& b,unsigned entry) {
    std::array<VehicleParameters,6> parameters{};
    std::array<VehicleState,6> physical{};
    std::array<VehicleRaceState,6> race{};
    std::array<contact::solver::State,6> contacts{};
    std::array<VehicleRaceDriverSettings,6> drivers{};
    std::array<AiAutomaticMemory,6> ai{};
    std::array<LevelCarSettings,6> cars{};
    for(unsigned i=0;i<6;++i) {
        auto bytes=std::span(b).subspan(i*record,record);
        contact::oracle::transfer(bytes,parameters[i],physical[i],race[i],contacts[i],true);
        drivers[i]={real(b,i*record+0x1b8c),real(b,i*record+0x1b90)};
        ai[i].steering_ramp_ticks=std::bit_cast<std::int32_t>(get(b,i*record+0x26a4));
        cars[i].ai_skill=get(b,tail+48+i*8);cars[i].ai_mode=get(b,tail+52+i*8);
    }
    unsigned current=get(b,tail);
    if(current>=6)throw std::runtime_error("bad current race slot");
    contact::player::Controls controls{};controls.current=get(b,tail+4);
    if(controls.current>=4)throw std::runtime_error("bad current race controller");
    for(unsigned i=0;i<4;++i) {
        controls.controllers[i].vehicle=get(b,tail+20+i*4);
        if(controls.controllers[i].vehicle>=6)throw std::runtime_error("bad controller vehicle");
    }
    RaceClockState clock{};clock.phase=get(b,tail+8);clock.race_mode=get(b,tail+12);
    Presentation presentation(b);
    RaceStartWorld world{physical,race,drivers,ai,cars,clock,controls,current,
        {real(b,tail+36),real(b,tail+40),real(b,tail+44)},presentation};
    switch(entry) {
    case 0x405880:choose_race_torque_curve(drivers[current],physical[current],world.torque_thresholds);break;
    case 0x4058f0:configure_race_drivers(world);break;
    case 0x444c20:begin_race_timers(world);break;
    case 0x412780:hold_start_vehicles(physical,race,clock);break;
    case 0x444c50:tick_race_countdown(world);break;
    case 0x443e20:tick_vehicle_notice(world);break;
    case 0x40e360:refill_vehicle(parameters[current],physical[current]);break;
    case 0x40e380:refill_impact_energy(physical[current],contacts[current]);break;
    default:throw std::runtime_error("unknown race start entry");
    }
    for(unsigned i=0;i<6;++i) {
        auto bytes=std::span(b).subspan(i*record,record);
        contact::oracle::transfer(bytes,parameters[i],physical[i],race[i],contacts[i],false);
        put(b,i*record+0x1b8c,std::bit_cast<unsigned>(drivers[i].skill));
        put(b,i*record+0x1b90,std::bit_cast<unsigned>(drivers[i].mode));
        put(b,i*record+0x26a4,std::bit_cast<unsigned>(ai[i].steering_ramp_ticks));
    }
    put(b,tail,current);put(b,tail+8,clock.phase);put(b,tail+12,clock.race_mode);
}
void read(std::istream& in,void* ptr,std::size_t count) {
    if(!in.read(static_cast<char*>(ptr),static_cast<std::streamsize>(count)))throw std::runtime_error("truncated race start corpus");
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected race start corpus path");
        std::ifstream in(argv[1],std::ios::binary);std::array<char,8> magic{};read(in,magic.data(),8);
        if(std::string_view(magic.data(),8)!="SGNRS001")throw std::runtime_error("wrong race start corpus version");
        unsigned count{};read(in,&count,4);if(count==0||count>10000)throw std::runtime_error("invalid race start case count");
        std::uint64_t words=0,differences=0;
        for(unsigned n=0;n<count;++n) {
            unsigned entry{};read(in,&entry,4);Bytes actual(size),expected(size);
            read(in,actual.data(),size);read(in,expected.data(),size);apply(actual,entry);
            for(unsigned at=0;at<size;at+=4) {
                ++words;
                if(get(actual,at)!=get(expected,at)) {
                    if(differences<12)std::cerr<<"case="<<n<<" entry="<<std::hex<<entry<<" at="<<at<<" got="<<get(actual,at)<<" expected="<<get(expected,at)<<std::dec<<'\n';
                    ++differences;
                }
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing race start corpus");
        std::cout<<"{\"passed\":"<<(differences?"false":"true")<<",\"cases\":"<<count
                 <<",\"compared_words\":"<<words<<",\"mismatches\":"<<differences<<"}\n";
        return differences?1:0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
