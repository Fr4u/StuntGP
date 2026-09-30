#include "sgn/retail_vehicle_load.hpp"
#include <iostream>
#include <memory>
using namespace sgn;
using namespace sgn::retail;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F>void rejects(F&& f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("invalid input accepted");}
}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("asset root required");
    const auto sounds=decode_vehicle_sound_ids("-SFX[] =\nEngine_Rev=12\nEngine_Idle=-3\nEngine_Boost=+4\nHorn=0\n");
    check(sounds.engine_rev==12&&sounds.engine_idle==-3&&sounds.engine_boost==4&&sounds.horn==0,"SFX order/signs");
    rejects([]{decode_vehicle_sound_ids("Engine_Rev=3");});
    rejects([]{decode_vehicle_sound_ids("-SFX[] =\nEngine_Rev=2147483648\n");});
    rejects([]{decode_vehicle_sound_ids("-SFX[] =\nEngine_Rev=+-1\nEngine_Idle=0\nEngine_Boost=0\nHorn=0\n");});
    AssetStore assets(argv[1]);auto p=std::make_unique<std::array<VehicleParameters,6>>();
    auto s=std::make_unique<std::array<VehicleState,6>>();std::array<VehicleRaceState,6>r{};
    std::array<VehiclePositionHistory,6>h{};std::array<VehicleLoadState,6>l{};
    for(unsigned i=0;i<6;++i){(*p)[i].fuel_capacity=100+float(i);(*p)[i].refuel_rate=2+float(i);
        (*p)[i].manual_gears=17;(*s)[i].position={float(i),2,3,4};(*s)[i].torque_curve=2;(*s)[i].brake_signal=27;
        r[i].active=1;r[i].control_enabled=0;h[i].ticks=123;h[i].samples[0].radius=55;l[i].current_refuel_rate=-9;}
    std::array<VehicleLoadVehicle,6>v{{{(*p)[0],(*s)[0],r[0],h[0],l[0]},
        {(*p)[1],(*s)[1],r[1],h[1],l[1]},{(*p)[2],(*s)[2],r[2],h[2],l[2]},
        {(*p)[3],(*s)[3],r[3],h[3],l[3]},{(*p)[4],(*s)[4],r[4],h[4],l[4]},
        {(*p)[5],(*s)[5],r[5],h[5],l[5]}}};
    unsigned current=3;VehicleLoadWorld world{v,current};
    load_vehicle_configuration(world,assets,4,1,-1);
    check(current==4&&r[4].control_kind==-1&&r[4].control_enabled==1&&(*s)[4].brake_signal==1,"loader current/control semantics");
    for(unsigned i=0;i<6;++i){check((*p)[i].manual_gears==17,"loader erased untouched manual gears");
        check(h[i].ticks==0&&(*s)[i].fuel==(*p)[i].fuel_capacity&&l[i].current_refuel_rate==(*p)[i].refuel_rate,"all-six history/fuel reset");
        for(const auto& sample:h[i].samples)check(sample.position.x==float(i)&&sample.position.w==4&&sample.radius==0,"history position/radius reset");
        if(i!=4)check((*s)[i].brake_signal==27&&r[i].control_enabled==0,"loader modified another vehicle controller");}
    rejects([&]{load_vehicle_configuration(world,assets,6,1,0);});check(current==4,"invalid target changed current");
    std::cout<<"{\"passed\":true,\"sound_fields\":true,\"untouched_parameters_preserved\":true,\"all_six_histories_reset\":true}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
