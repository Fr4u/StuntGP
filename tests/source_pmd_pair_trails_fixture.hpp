#pragma once
#include "source_pmd_speed_trail_fixture.hpp"
#include "sgn/vehicle_trail_append.hpp"
namespace sgn::pair_trail_test {
using namespace retail;
using speed_trail_test::Record;using speed_trail_test::size;using speed_trail_test::tick_at;
using speed_trail_test::word;using speed_trail_test::real;using speed_trail_test::put;using speed_trail_test::load;
using speed_trail_test::read;using speed_trail_test::mask;using speed_trail_test::Fixture;using speed_trail_test::check;
using speed_trail_test::rejects;using speed_trail_test::snapshot;using speed_trail_test::transform;using speed_trail_test::pointer;
//Inherit unchanged full transfer/pack/mask helpers; only the pair binding is new.
struct State: speed_trail_test::State {
    State(){tick=60;inputs.field_28a0=1;physics.position.z=1000;projection={vehicle_identity(),1,1,100,100,80,80,1};
        inputs.pair_a={VehicleVector{-3,4,5,0},VehicleVector{6,7,8,0}};
        inputs.pair_b={VehicleVector{9,10,11,0},VehicleVector{12,13,14,0}};
        for(unsigned j=0;j<2;++j){storage.cursor_a[j]=j+2;storage.cursor_b[j]=j+3;
            physics.camera_effects.values[4+j]=j+3;physics.camera_effects.values[6+j]=j+4;
            for(unsigned i=0;i<32;++i){storage.pair_a[j][i]={float(i+3),float(i+7),float(i+11),std::bit_cast<float>(0x7fc01000u+j*64+i)};
                storage.pair_b[j][i]={float(i+13),float(i+17),float(i+19),std::bit_cast<float>(0x7fc02000u+j*64+i)};}}
    }
    VehiclePairTrailBindings pair_binding(){return {physics,storage,scratch,inputs,projection,settings,tick,audit};}
};
inline void source(State& s,bool b,ModelResources& models,std::size_t handle){
    if(b)update_vehicle_pair_b_trails(s.pair_binding(),models,handle);else update_vehicle_pair_a_trails(s.pair_binding(),models,handle);
}
inline void source(State& s,bool b,SourcePmdModelView model){
    if(b)update_vehicle_pair_b_trails(s.pair_binding(),model);else update_vehicle_pair_a_trails(s.pair_binding(),model);
}
inline void legacy(State& s,bool b){if(b)update_vehicle_pair_b_trails(s.legacy_binding());else update_vehicle_pair_a_trails(s.legacy_binding());}
inline void append_one(State& s,bool b,unsigned i){auto point=b?s.inputs.pair_b[i]:s.inputs.pair_a[i];
    if(b)point.y=vehicle_f32(double(point.y)-20.0);
    append_vehicle_trail_point(s.legacy_binding(),point,b?s.storage.pair_b[i]:s.storage.pair_a[i],b?s.storage.cursor_b[i]:s.storage.cursor_a[i],
        s.physics.camera_effects.values[(b?6u:4u)+i],b?s.settings.pair_b:s.settings.pair_a);
}
inline VehicleTrailModel observe_basis(SourcePmdModelView model){const auto bytes=model.bytes();const auto first=transform(model,0);
    return {bytes.field<VehicleVector>(first+32).value(),bytes.field<VehicleVector>(first+48).value(),0};
}
inline void project_basis(Fixture& fixture,const VehicleTrailModel& input){const auto model=fixture.view();const auto bytes=model.bytes();const auto first=transform(model,0);
    bytes.field<VehicleVector>(first+32).set(input.base_forward);bytes.field<VehicleVector>(first+48).set(input.base_up);
}
inline std::uint64_t unchanged_pmd(Fixture& f,const Bytes& before){const auto after=snapshot(f.bank,f.view().allocation());check(before==after,"pair trail modified PMD payload");return after.size();}
inline void validate_record(const Record& b,bool second){
    if(word(b,tick_at)<60||word(b,0x28a0)==0)return; //do not admit unused observations ahead of either gate
    for(unsigned at:{0x288cu,0x1bacu,0x1bb0u,0x1bb4u,0x1cbcu,0x1cc0u,0x1cc4u,0x1cccu,0x1cd0u,0x1cd4u,0x1cdcu,0x1ce0u,0x1ce4u,
        0x7800u,0x7804u,0x7808u,0x7810u,0x7814u,0x7818u})check(std::isfinite(real(b,at)),"pair trail corpus outside finite domain");
    for(unsigned i:{0u,1u,2u,4u,5u,6u,8u,9u,10u,12u,13u,14u})check(std::isfinite(real(b,0x7880+i*4)),"pair trail corpus outside finite domain");
    for(unsigned i=0;i<7;++i)check(std::isfinite(real(b,0x78c0+i*4)),"pair trail corpus outside finite domain");
    for(unsigned base:second?std::array<unsigned,2>{0x28c0,0x2adc}:std::array<unsigned,2>{0x2d04,0x2f1c})
        for(unsigned j=0;j<3;++j)check(std::isfinite(real(b,base+j*4)),"pair trail corpus outside finite domain");
}
}
