#pragma once
#include "source_pmd_brake_lights_fixture.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
namespace sgn::speed_trail_test {
using namespace retail;
using brake_test::check;using brake_test::rejects;using brake_test::Fixture;using brake_test::snapshot;
//The transfer and mask below are the existing immutable legacy reader helpers.
constexpr std::size_t size=0x8000,model_at=0x7800,scratch_at=0x7840,view_at=0x7880,projection_at=0x78c0,tick_at=0x7900;
using Record=std::array<std::uint8_t,size>;
template<class T> void load(std::ifstream& s,T& v){s.read(reinterpret_cast<char*>(&v),sizeof(v));if(!s)throw std::runtime_error("truncated trail corpus");}
inline std::uint32_t read(std::ifstream& s){std::uint32_t v{};load(s,v);return v;}
inline std::uint32_t word(const Record& b,std::size_t at){if(at>size-4)throw std::runtime_error("trail offset outside record");std::uint32_t v{};std::memcpy(&v,b.data()+at,4);return v;}
inline void put(Record& b,std::size_t at,std::uint32_t v){if(at>size-4)throw std::runtime_error("trail offset outside record");std::memcpy(b.data()+at,&v,4);}
inline float real(const Record& b,std::size_t at){return std::bit_cast<float>(word(b,at));}
inline void transfer(Record& b,VehicleState& s,VehicleParameters& p,VehicleTrailStorage& trails,VehicleTrailInputs& input,
              VehicleTrailModel& model,VehicleMatrix& scratch,VehicleTrailProjection& projection,bool reading) {
    auto u=[&](std::uint32_t& x,std::size_t at){if(reading)x=word(b,at);else put(b,at,x);};
    auto f=[&](float& x,std::size_t at){if(reading)x=real(b,at);else put(b,at,std::bit_cast<std::uint32_t>(x));};
    auto v=[&](VehicleVector& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);f(x.w,at+12);};
    auto m=[&](VehicleMatrix& x,std::size_t at){for(unsigned k=0;k<16;++k)f(x[k],at+k*4);};
    f(p.height,0x244);f(s.speed,0x1cf0);f(s.body_heave,0x288c);v(s.position,0x1bac);v(s.basis_x,0x1cbc);v(s.basis_z,0x1cdc);
    f(s.up.x,0x1ccc);f(s.up.y,0x1cd0);f(s.up.z,0x1cd4);u(input.field_28a0,0x28a0);
    for(unsigned k=0;k<64;++k)v(trails.speed[k],0x351c+k*16);
    u(s.camera_effects.values[2],0x391c);u(s.camera_effects.values[3],0x3920);
    constexpr std::array<std::size_t,2> ar{0x2d14,0x2f2c},br{0x28d0,0x2aec};
    constexpr std::array<std::size_t,2> ai{0x2d04,0x2f1c},bi{0x28c0,0x2adc};
    constexpr std::array<std::size_t,2> ac{0x2f14,0x312c},bc{0x2ad0,0x2cec},bf{0x28bc,0x2ad8};
    for(unsigned j=0;j<2;++j) {
        v(input.pair_a[j],ai[j]);v(input.pair_b[j],bi[j]);u(input.pair_b_available[j],bf[j]);
        u(trails.cursor_a[j],ac[j]);u(trails.cursor_b[j],bc[j]);
        u(s.camera_effects.values[4+j],ac[j]+4);u(s.camera_effects.values[6+j],bc[j]+4);
        for(unsigned k=0;k<32;++k){v(trails.pair_a[j][k],ar[j]+k*16);v(trails.pair_b[j][k],br[j]+k*16);}
    }
    v(model.base_forward,model_at);v(model.base_up,model_at+16);f(model.part23_position_z,model_at+32);
    m(scratch,scratch_at);m(projection.world_view,view_at);
    f(projection.projection_x,projection_at);f(projection.projection_y,projection_at+4);
    f(projection.center_x,projection_at+8);f(projection.center_y,projection_at+12);
    f(projection.scale_x,projection_at+16);f(projection.scale_y,projection_at+20);f(projection.clip_near,projection_at+24);
}
inline std::set<std::size_t> mask(std::uint32_t entry,const Record& b) {
    std::set<std::size_t> result;
    auto ring=[&](std::size_t at,std::size_t cursor,std::uint32_t capacity) {
        const auto next=(word(b,cursor)+1u)%capacity;
        result.insert(scratch_at+12);result.insert(scratch_at+28);
        result.insert(at+capacity*16+12);result.insert(at+next*16+12);
    };
    if((entry==0x414c60 || entry==1) && word(b,tick_at)>=20 &&
       double(real(b,0x1cf0))-std::bit_cast<float>(std::uint32_t{0x42855555})>=0)
        ring(0x351c,0x391c,16);
    if(word(b,tick_at)>=60 && word(b,0x28a0)!=0) {
        if(entry==0x414f90 || entry==1){ring(0x2d14,0x2f14,5);ring(0x2f2c,0x312c,5);}
        if(entry==0x4152d0 || entry==1){ring(0x28d0,0x2ad0,9);ring(0x2aec,0x2cec,9);}
    }
    return result;
}

struct State {
    VehicleState physics{};VehicleParameters parameters{};VehicleTrailStorage storage{};VehicleTrailInputs inputs{};
    VehicleTrailModel model{};VehicleMatrix scratch{};VehicleTrailProjection projection{};
    VehicleTrailSettings settings=initial_vehicle_trail_settings();unsigned tick=20;VehicleTrailWriteAudit* audit{};
    State(){physics.speed=100;parameters.height=55;physics.position={10,20,30,0};physics.basis_x={1,0,0,0};physics.basis_z={0,0,1,0};physics.up={0,1,0};
        physics.camera_effects.values[2]=5;physics.camera_effects.values[3]=7;scratch=vehicle_identity();
        for(unsigned i=0;i<storage.speed.size();++i)storage.speed[i]={float(i),float(i+1),float(i+2),std::bit_cast<float>(0x3f000000u+i)};
    }
    void load_record(Record& b){transfer(b,physics,parameters,storage,inputs,model,scratch,projection,true);tick=word(b,tick_at);}
    void pack(Record& b){transfer(b,physics,parameters,storage,inputs,model,scratch,projection,false);put(b,tick_at,tick);}
    Record record(){Record b{};b.fill(0xa5);pack(b);return b;}
    VehicleSpeedTrailBindings binding(){return {physics,parameters,storage,scratch,projection,settings,tick,audit};}
    VehicleTrailBindings legacy_binding(){return {physics,parameters,storage,scratch,inputs,model,projection,settings,tick,audit};}
};
inline unsigned transform(SourcePmdModelView model,unsigned part){return model.part(model.source_lod_index(0),part).transform().location().offset;}
inline void project_model(Fixture& fixture,const VehicleTrailModel& input){const auto model=fixture.view();const auto bytes=model.bytes();const auto first=transform(model,0),last=transform(model,23);
    bytes.field<VehicleVector>(first+0x20).set(input.base_forward);bytes.field<VehicleVector>(first+0x30).set(input.base_up);bytes.field<float>(last+8).set(input.part23_position_z);
}
inline VehicleTrailModel observe(SourcePmdModelView model){const auto bytes=model.bytes();const auto z=bytes.field<float>(transform(model,23)+8).value();const auto first=transform(model,0);
    return {bytes.field<VehicleVector>(first+32).value(),bytes.field<VehicleVector>(first+48).value(),z};
}
inline void pointer(SourcePmdModelView model,unsigned part,unsigned value){const auto p=model.part(model.source_lod_index(0),part);model.bytes().field<unsigned>(p.location().offset+8).set(value);}
inline void validate_record(const Record& b){
    for(unsigned at:{0x244u,0x1cf0u,0x288cu,0x1bacu,0x1bb0u,0x1bb4u,0x1cbcu,0x1cc0u,0x1cc4u,0x1cccu,0x1cd0u,0x1cd4u,0x1cdcu,0x1ce0u,0x1ce4u,0x7800u,0x7804u,0x7808u,0x7810u,0x7814u,0x7818u,0x7820u})
        check(std::isfinite(real(b,at)),"speed trail corpus outside finite domain");
}
inline std::uint64_t unchanged_pmd(Fixture& f,const Bytes& before){const auto after=snapshot(f.bank,f.view().allocation());check(before==after,"speed trail modified PMD payload");return after.size();}
}
