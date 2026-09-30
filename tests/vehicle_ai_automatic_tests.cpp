#include "sgn/vehicle_ai_automatic.hpp"
#include <fstream>
#include <iostream>
#include <memory>

using namespace sgn;
using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Bytes file(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("missing model");
    auto size=f.tellg();Bytes b(static_cast<std::size_t>(size));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),size);require(bool(f),"short model");return b;}
struct Fixture {
    std::array<VehicleParameters,6> p{};std::array<VehicleState,6>s{};
    std::array<VehicleRaceState,6>r{};std::array<AiAutomaticMemory,6>a{};
    std::array<AiAutomaticVehicle,6> cars{{{p[0],s[0],r[0],a[0]},{p[1],s[1],r[1],a[1]},
        {p[2],s[2],r[2],a[2]},{p[3],s[3],r[3],a[3]},{p[4],s[4],r[4],a[4]},{p[5],s[5],r[5],a[5]}}};
};
}
int main(int argc,char**argv){try{
    require(argc==2,"usage: vehicle_ai_automatic_tests model_directory");
    SplineSet decoded(file(std::filesystem::path(argv[1])/"track1.PMD"));const DecodedTrackSplines set{decoded,{}};auto fixture=std::make_unique<Fixture>();auto& x=*fixture;
    AiAutomaticGlobals g;g.initialized=2;g.target_speed=123;
    for(unsigned j=0;j<6;++j){x.r[j].route_indices.fill(7);x.r[j].route_fractions.fill(12.5f);x.s[j].stunt_meter=9;
        x.s[j].upgrade_condition=0.4f;x.a[j].reverse_ticks=8;x.a[j].counter2674=7;x.a[j].counter2694=6;
        x.a[j].passing_ticks=5;x.a[j].following_ticks=4;x.a[j].bend=3;x.a[j].lateral_fraction=-0.25f;x.a[j].steering_ramp_ticks=300;}
    initialize_ai_automatic(set,x.cars,g);
    require(g.initialized==1&&bool(g.routes)&&g.target_speed==123,"initialization unrelated state");
    for(unsigned j=0;j<6;++j){require(x.s[j].stunt_meter==0&&x.r[j].route_indices==std::array<std::int32_t,3>{},"all six shared fields reset");
        require(x.r[j].route_fractions==std::array<float,3>{}&&x.a[j].reverse_ticks==0&&x.a[j].counter2674==0&&x.a[j].counter2694==0,"AI init reset");
        require(x.a[j].bend==3&&x.a[j].lateral_fraction==-0.25f&&x.a[j].steering_ramp_ticks==300&&x.s[j].upgrade_condition==0.4f,"untouched fields preserved");}
    x.s[4].stunt_meter=77;initialize_ai_automatic(set,x.cars,g);require(x.s[4].stunt_meter==77,"exactly-once cache gate");
    AiAutomaticEnvironment env{{0,1,0},41.666667f,25,4};
    x.r[0].active=1;x.s[0].special_mode=1;x.s[0].speed=6;x.a[0].steering_ramp_ticks=1;
    ai_automatic_tick(set,x.cars,0,g,env);require(x.a[0].steering_ramp_ticks==0,"ramp update precedes special-mode return");
    x.s[0].special_mode=0;x.r[0].automatic_drive=0;bool rejected=false;
    try{ai_automatic_tick(set,x.cars,0,g,env);}catch(const FormatError&){rejected=true;}require(rejected,"ordinary rival branch must be explicit unsupported");
    x.r[0].automatic_drive=1;env.game_type=3;x.s[0].turbo_active=1;x.s[0].gear=0;
    ai_automatic_tick(set,x.cars,0,g,env);require(x.s[0].gear==0&&x.s[0].turbo_active==1,"challenge gate preserves automatic writes");
    env.game_type=4;const auto spline=decoded.find(1);require(spline.has_value(),"track primary route");
    x.s[0].position=spline_point(*spline,10).value;x.s[0].basis_x={1,0,0,0};x.s[0].basis_z={0,0,1,0};x.s[0].up={0,1,0};
    x.p[0].steering_max=0.4f;x.r[0].control_enabled=1;x.s[0].speed=0;
    ai_automatic_tick(set,x.cars,0,g,env);
    require(x.s[0].gear==2&&x.s[0].turbo_active==0&&x.s[0].upgrade_condition==1,"shared physical automatic writes");
    require(x.r[0].control_enabled==0&&x.s[0].throttle==1,"real disable-control then gas sequence");
    require(g.braking_allowed==1&&g.target_speed==25,"automatic target speed");
    AiAutomaticGlobals missing;missing.initialized=1;rejected=false;
    try{sample_ai_route(missing,0,0);}catch(const FormatError&){rejected=true;}require(rejected,"missing route is not a fake position");
    std::cout<<"automatic-drive shared ownership, init, guards and dependency contracts passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
