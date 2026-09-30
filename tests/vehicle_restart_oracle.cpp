#include "sgn/vehicle_reset.hpp"
#include "vehicle_restart_state.hpp"
#include "contact_solver_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
using namespace sgn::retail;
using Record=std::vector<std::uint8_t>;
constexpr unsigned size=0x30000,cs=0x7800,ca=0x2d000,p=0x2f000;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void put(Record& b,unsigned at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::int32_t i(const Record& b,unsigned at){return std::bit_cast<std::int32_t>(u(b,at));}
void apply(Record& b) {
    auto parameters=std::make_unique<std::array<VehicleParameters,6>>();
    auto physics=std::make_unique<std::array<VehicleState,6>>();
    auto contact=std::make_unique<std::array<contact::solver::State,6>>();
    std::array<VehicleRaceState,6> races{};
    std::array<VehicleRestartState,6> restarts{};
    auto transfer=[&](bool read) {for(unsigned j=0;j<6;++j) {
        auto bytes=std::span(b).subspan(j*cs,cs);
        contact::oracle::transfer(bytes,(*parameters)[j],(*physics)[j],races[j],(*contact)[j],read);
        restart_oracle::transfer(bytes,restarts[j],read);
    }};
    transfer(true);
    std::array<ResetVehicle,6> vehicles{{
        {(*physics)[0],races[0],(*contact)[0],0},{(*physics)[1],races[1],(*contact)[1],1},
        {(*physics)[2],races[2],(*contact)[2],2},{(*physics)[3],races[3],(*contact)[3],3},
        {(*physics)[4],races[4],(*contact)[4],4},{(*physics)[5],races[5],(*contact)[5],5}}};
    contact::player::Controls controls;controls.current=u(b,p+0x34);
    auto controllers=[&](bool read) {for(unsigned j=0;j<4;++j) {
        const auto at=ca+j*0x74c;auto& c=controls.controllers[j];
        if(read) {
            c.active=i(b,at);c.vehicle=u(b,at+0xac);c.camera.mode=u(b,at+4);
            c.camera.twist=f(b,at+0x78);c.camera.collision_bypass_ticks=u(b,at+0x1a4);
            c.respawn_transition.state=u(b,at+0x300);c.respawn_transition.phase=u(b,at+0x308);
            std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));
        }else {
            put(b,at,std::bit_cast<std::uint32_t>(c.active));put(b,at+0xac,c.vehicle);put(b,at+4,c.camera.mode);
            put(b,at+0x78,std::bit_cast<std::uint32_t>(c.camera.twist));put(b,at+0x1a4,c.camera.collision_bypass_ticks);
            put(b,at+0x300,c.respawn_transition.state);put(b,at+0x308,c.respawn_transition.phase);
            std::memcpy(b.data()+at+0x4ac,c.notices.data(),sizeof(c.notices));
        }
    }};
    if(controls.current>=4)throw std::runtime_error("restart controller identity");
    controllers(true);
    for(const auto& c:controls.controllers) {
        if(c.vehicle>=6)throw std::runtime_error("restart controller vehicle identity");
        for(const auto& notice:c.notices)if(notice.source_vehicle>=6)throw std::runtime_error("restart notice vehicle identity");
    }
    VehicleResetParameters config{};config.stage=i(b,p+8);config.mode=i(b,p+12);config.selector=i(b,p+16);
    config.tick=u(b,p+20);config.normal_energy_cost=f(b,p+24);config.upgraded_energy_cost=f(b,p+28);
    config.course_length=f(b,p+32);config.spacing_word=u(b,p+36);
    for(unsigned j=0;j<128;++j)config.mask[j]=f(b,p+0x100+j*4);
    config.unit_gravity={f(b,p+0x40),f(b,p+0x44),f(b,p+0x48),f(b,p+0x4c)};
    config.vehicle_names={"CAR 0","CAR 1","CAR 2","CAR 3","CAR 4","CAR 5"};
    config.special_end_text="Recovered";config.retired_text="Retired";
    VehicleResetGlobals globals{u(b,p+0x28),u(b,p+0x2c),u(b,p+0x30)};
    VehicleResetWorld world{vehicles,u(b,p+4),controls,globals,config};const auto target=u(b,p);
    if(target>=6 || world.current>=6)throw std::runtime_error("reset corpus slot");
    restart_vehicle(world,target,restarts[target]);
    transfer(false);controllers(false);put(b,p+4,world.current);put(b,p+0x34,controls.current);
    put(b,p+0x28,globals.transition_active);put(b,p+0x2c,globals.special_value);put(b,p+0x30,globals.special_timer);
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short reset corpus");}
unsigned word(std::ifstream& in){unsigned x;read(in,&x,4);return x;}
}
int main(int argc,char** argv) {try {
    if(argc!=2)throw std::runtime_error("expected restart corpus");
    std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
    if(std::string(magic,8)!="SGNRE001")throw std::runtime_error("restart corpus schema");
    const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("restart corpus count");
    std::uint64_t exact=0,errors=0;
    for(unsigned n=0;n<count;++n) {
        if(word(in)!=0x413530)throw std::runtime_error("unknown restart entry");
        Record actual(size),expected(size);read(in,actual.data(),size);read(in,expected.data(),size);
        if(u(actual,p)>=6 || u(actual,p+4)>=6)throw std::runtime_error("restart corpus slot before state access");
        apply(actual);
        for(unsigned at=0;at<size;at+=4) {
            if(u(actual,at)==u(expected,at)){++exact;continue;}
            if(errors++<24)std::cerr<<"case "<<n<<" offset "<<std::hex<<at<<" expected "<<u(expected,at)<<" actual "<<u(actual,at)<<std::dec<<'\n';
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing restart corpus");
    std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"allowed_ulp\":0,\"excluded_words\":0}\n";
    return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
