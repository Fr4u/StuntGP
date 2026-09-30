#include "sgn/vehicle_reset.hpp"
#include "sgn/vehicle_reset_tick.hpp"
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
bool apply(Record& b,unsigned entry,SplineView spline,bool missing_spacing=false) {
    auto parameters=std::make_unique<std::array<VehicleParameters,6>>();
    auto physics=std::make_unique<std::array<VehicleState,6>>();
    auto contact=std::make_unique<std::array<contact::solver::State,6>>();
    std::array<VehicleRaceState,6> races{};
    auto transfer=[&](bool read) {for(unsigned j=0;j<6;++j) {
        auto bytes=std::span(b).subspan(j*cs,cs);
        contact::oracle::transfer(bytes,(*parameters)[j],(*physics)[j],races[j],(*contact)[j],read);
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
    controllers(true);
    VehicleResetParameters config{};config.stage=i(b,p+8);config.mode=i(b,p+12);config.selector=i(b,p+16);
    config.tick=u(b,p+20);config.normal_energy_cost=f(b,p+24);config.upgraded_energy_cost=f(b,p+28);
    config.course_length=f(b,p+32);config.spacing_word=u(b,p+36);config.spline=spline;
    if(missing_spacing)config.spacing_word.reset();
    for(unsigned j=0;j<128;++j)config.mask[j]=f(b,p+0x100+j*4);
    config.unit_gravity={f(b,p+0x40),f(b,p+0x44),f(b,p+0x48),f(b,p+0x4c)};
    config.vehicle_names={"CAR 0","CAR 1","CAR 2","CAR 3","CAR 4","CAR 5"};
    config.special_end_text="Recovered";config.retired_text="Retired";
    VehicleResetGlobals globals{u(b,p+0x28),u(b,p+0x2c),u(b,p+0x30)};
    VehicleResetWorld world{vehicles,u(b,p+4),controls,globals,config};const auto target=u(b,p);
    if(target>=6 || world.current>=6)throw std::runtime_error("reset corpus slot");
    bool geometry=false;
    if(entry==0x418710)consume_respawn_energy(vehicles[target],config);
    else if(entry==0x418760)put(b,p+0x38,exhausted_and_still(vehicles[target].physics));
    else if(entry==0x4186c0)retire_current_vehicle(world);
    else if(entry==0x4187a0)check_vehicle_retirement(world);
    else if(entry==0x418d10)clear_special_globals(globals);
    else if(entry==0x418d20)prepare_special_respawn(world,target);
    else if(entry==0x418d90)end_vehicle_special_mode(world,target);
    else if(entry==0x412940)geometry=reset_vehicle(world,target);
    else if(entry==0x408d80)clear_accumulated_impact_loss(vehicles[world.current].contact);
    else if(entry==0x4187c0)check_stuck_vehicle(vehicles[world.current].physics,vehicles[world.current].race);
    else if(entry==0x418320) {
        VehicleResetBounds bounds;
        for(unsigned j=0;j<256;++j){bounds.left[j]=f(b,p+0x300+j*4);bounds.right[j]=f(b,p+0x700+j*4);}
        geometry=finish_vehicle_bounds(world,bounds);
    }
    else throw std::runtime_error("unknown vehicle reset entry");
    transfer(false);controllers(false);put(b,p+4,world.current);put(b,p+0x34,controls.current);
    put(b,p+0x28,globals.transition_active);put(b,p+0x2c,globals.special_value);put(b,p+0x30,globals.special_timer);
    return geometry;
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short reset corpus");}
unsigned word(std::ifstream& in){unsigned x;read(in,&x,4);return x;}
}
int main(int argc,char** argv) {
    try {
        if(argc<3||argc>4||(argc==4&&std::string_view(argv[3])!="--missing-spacing"))throw std::runtime_error("expected reset corpus and PMD [--missing-spacing]");
        SplineSet set(sgn::read_file(argv[2]));if(set.descriptors().empty())throw std::runtime_error("missing reset spline");
        const auto spline=*set.find(set.descriptors().front().id);
        std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
        if(std::string(magic,8)!="SGNRS001")throw std::runtime_error("reset corpus schema");
        const auto count=word(in);if(count==0 || count>20000)throw std::runtime_error("reset corpus count");
        std::uint64_t exact=0,excluded=0;unsigned errors=0;
        for(unsigned n=0;n<count;++n) {
            const auto entry=word(in);Record actual(size),expected(size);read(in,actual.data(),size);read(in,expected.data(),size);
            if(u(actual,p)>=6 || u(actual,p+4)>=6)throw std::runtime_error("reset corpus slot before state access");
            const auto omissions=word(in);if(omissions>5)throw std::runtime_error("reset opaque count");
            std::vector<unsigned> opaque;
            for(unsigned j=0;j<omissions;++j) {
                const auto offset=word(in);const auto car=offset/cs,local=offset%cs;
                const auto expected_car=entry==0x418320?u(actual,p+4):u(actual,p);
                if((entry!=0x412940 && entry!=0x418320) || car!=expected_car ||
                   (local!=0x1bd8 && local!=0x1bf8 && local!=0x24cc && local!=0x72c4 && local!=0x72e4) ||
                   std::find(opaque.begin(),opaque.end(),offset)!=opaque.end())throw std::runtime_error("invalid reset opaque exclusion");
                opaque.push_back(offset);
            }
            const auto target=entry==0x418320?u(actual,p+4):u(actual,p);const auto old_right=u(actual,target*cs+0x1bd8),old_heading=u(actual,target*cs+0x24cc);
            const bool geometry=apply(actual,entry,spline,argc==4);
            if(omissions!=(geometry?5u:0u))throw std::runtime_error("reset opaque mask disagrees with executed placement path");
            if(geometry) {
                for(const auto off:{0x1bd8u,0x72c4u})if(u(actual,target*cs+off)!=old_right)throw std::runtime_error("reset right opaque policy");
                for(const auto off:{0x1bf8u,0x24ccu,0x72e4u})if(u(actual,target*cs+off)!=old_heading)throw std::runtime_error("reset heading opaque policy");
            }
            for(unsigned offset=0;offset<size;offset+=4) {
                if(std::find(opaque.begin(),opaque.end(),offset)!=opaque.end()){++excluded;continue;}
                if(u(actual,offset)==u(expected,offset)){++exact;continue;}
                if(errors++<30)std::cerr<<"entry "<<std::hex<<entry<<" case "<<std::dec<<n<<" offset "<<std::hex<<offset<<" actual "<<u(actual,offset)<<" expected "<<u(expected,offset)<<'\n';
            }
        }
        if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing reset corpus bytes");
        std::cout<<"{\"cases\":"<<count<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"excluded_opaque_words\":"<<excluded<<",\"allowed_ulp\":0}\n";
        return errors?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
