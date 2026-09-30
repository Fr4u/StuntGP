#pragma once
#include "source_pmd_launch_original_fixture.hpp"
#include "source_pmd_brake_lights_fixture.hpp"
#include <map>
#include <iostream>
namespace sgn::launch_test {
using namespace retail;
using launch_original::Record;using launch_original::State;using launch_original::word;using launch_original::put;
using launch_original::transfer;using launch_original::policy;using launch_original::validate;using launch_original::read;
using brake_test::Fixture;using brake_test::snapshot;using brake_test::check;using brake_test::rejects;
inline unsigned section(SourcePmdModelView m,unsigned h){return 0x148+m.bytes().field<unsigned>(h).value();}
inline unsigned part_at(SourcePmdModelView m,unsigned raw,unsigned i){return m.part(m.source_lod_index(raw),i).location().offset;}
inline unsigned transform_at(SourcePmdModelView m,unsigned raw,unsigned i){return m.part(m.source_lod_index(raw),i).transform().location().offset;}
inline void member(SourcePmdModelView m,unsigned raw,unsigned i,unsigned offset,unsigned value){m.bytes().field<unsigned>(part_at(m,raw,i)+offset).set(value);}
inline void coord_span(SourcePmdModelView m,unsigned i,unsigned pointer,unsigned count){
    const auto p=part_at(m,0,i);m.bytes().field<unsigned>(p+0x2c).set(pointer);m.bytes().field<std::uint16_t>(p+0x26).set(static_cast<std::uint16_t>(count));
}
inline VehicleTrailModel basis(SourcePmdModelView m){const auto t=transform_at(m,0,0);const auto b=m.bytes();return {b.field<VehicleVector>(t+32).value(),b.field<VehicleVector>(t+48).value(),0};}
inline VehicleLaunchBindings binding(State& s,unsigned tick,VehicleLaunchAudit* audit=nullptr,VehicleTrailWriteAudit* trail=nullptr){return {s.physics,s.basis,s.projection,tick,trail,audit};}
inline void source(State& s,unsigned tick,Fixture& f,VehicleLaunchAudit* audit=nullptr,VehicleTrailWriteAudit* trail=nullptr){
    update_vehicle_launch_effect(binding(s,tick,audit,trail),s.effects,s.anchors,f.models,f.handle,s.particles,s.random);
}
inline Record packed(State& s){Record r{};r.fill(0xa5);transfer(r,s,false);return r;}
inline std::unique_ptr<State> state(){auto s=std::make_unique<State>();s->basis=vehicle_identity();s->projection={vehicle_identity(),1,1,0,0,1,1,1};
    auto& p=s->physics;p.position={10,20,30,0};p.basis_x={1,0,0,0};p.basis_z={0,0,1,0};p.up={0,1,0};p.turbo_rpm=20;p.launch_effect_level=10;p.launch_effect_strength=.2f;p.launch_effect_timer=1;
    p.velocity={2,3,4};p.velocity_w=std::bit_cast<float>(0x7fc01234u);s->anchors.effect_available={1,1};
    s->anchors.effect_position={VehicleVector{3,4,5,0},VehicleVector{6,7,8,0}};s->anchors.effect_base_scale={VehicleVector{1,2,3,0},VehicleVector{4,5,6,0}};
    s->random.state=12345;s->effects.enabled=7;s->effects.phase=8190;s->effects.cursor={3,4};p.camera_effects.values[0]=2;p.camera_effects.values[1]=3;
    for(unsigned i=0;i<2;++i){s->effects.current_scale[i]={8,9,10,std::bit_cast<float>(0x7fc04321u+i)};for(unsigned j=0;j<16;++j)s->effects.trails[i][j]={11,12,13,std::bit_cast<float>(0x7fc06000u+i*32+j)};}
    for(unsigned i=0;i<32;++i)s->particles.particles[i].position.opaque=0x7fc08000u+i;return s;
}
struct Projection {
    Fixture owner;unsigned coordinate_start{};std::set<unsigned> allowed;
    Projection(const std::filesystem::path& root,const Record& r):owner(root){
        validate(r);check(word(r,0x7e0c)==0,"launch standalone step must be zero");
        const auto m=owner.view();const auto b=m.bytes();const auto d=m.descriptor();const auto current=word(r,0x7e08);
        const auto raw0=word(r,0x7e20),selected=word(r,0x7e20+current*4);const auto h0=d.field<unsigned>(0x48).value(),h1=d.field<unsigned>(0x4c).value();check(h0!=h1,"launch fixture requires two physical ranges");
        for(unsigned slot=0;slot<4;++slot)d.field<unsigned>(0x48+slot*4).set(word(r,0x7e20+slot*4)==raw0?h0:h1);
        d.field<unsigned>(0x58).set(3);d.field<unsigned>(0x24).set(word(r,0x7e10));m.current_detail_level().set(current);
        std::map<unsigned,unsigned> mapped;const auto start=section(m,0x44);
        const auto map_transform=[&](unsigned group,unsigned index,unsigned raw){const auto id=word(r,0x13400+(group*24+index)*16);check(id<96,"launch transform identity");
            if(!mapped.contains(id)){const auto offset=start+static_cast<unsigned>(mapped.size())*80;mapped[id]=offset;SourcePmdTransform value;std::memcpy(&value,r.data()+0x11000+id*80,80);b.field<SourcePmdTransform>(offset).set(value);}
            member(m,raw,index,8,mapped.at(id));};
        map_transform(raw0,0,0);map_transform(selected,19,current);map_transform(selected,20,current);
        coordinate_start=section(m,0x28);check(b.field<unsigned>(0xbc).value()>=256*16,"car1 coordinates cannot hold original launch fixture");
        for(unsigned j=0;j<256;++j){std::array<unsigned,4> value;std::memcpy(value.data(),r.data()+0x14800+j*16,16);b.field<std::array<unsigned,4>>(coordinate_start+j*16).set(value);}
        const float turbo=std::bit_cast<float>(word(r,0x2254));const bool active=word(r,0x328c)==1&&!((turbo==0||std::isnan(turbo))&&word(r,0x2260)==0);
        for(unsigned i:{19u,20u}){const auto meta=word(r,0x13400+(raw0*24+i)*16+8),begin=meta&0xffff,count=meta>>16;check(begin+count<=256,"launch coordinate extent");
            coord_span(m,i,coordinate_start+begin*16,count);
            if(active&&(i==19||word(r,0x33cc)!=0))for(unsigned j=0;j<count;++j)for(unsigned k=0;k<4;++k)allowed.insert(coordinate_start+(begin+j)*16+4+k);
        }
    }
    void pack(State& s){const auto b=owner.view().bytes();for(unsigned i=0;i<256;++i)s.model.coords[i]=b.field<std::array<unsigned,4>>(coordinate_start+i*16).value();}
};
inline void finite_record(const Record& r){
    if(word(r,0x328c)!=1)return;const auto turbo=std::bit_cast<float>(word(r,0x2254));check(std::isfinite(turbo),"launch corpus outside finite domain");
    if(turbo==0&&word(r,0x2260)==0)return;
    for(unsigned at:{0x2264u,0x288cu,0x1bacu,0x1bb0u,0x1bb4u,0x1cbcu,0x1cc0u,0x1cc4u,0x1cccu,0x1cd0u,0x1cd4u,0x1cdcu,0x1ce0u,0x1ce4u})check(std::isfinite(std::bit_cast<float>(word(r,at))),"launch corpus outside finite domain");
    for(unsigned i=0;i<2;++i)for(unsigned j=0;j<3;++j){check(std::isfinite(std::bit_cast<float>(word(r,0x3290+i*0x140+j*4))),"launch corpus outside finite domain");check(std::isfinite(std::bit_cast<float>(word(r,0x32a0+i*0x140+j*4))),"launch corpus outside finite domain");}
    if(word(r,0x7e18)<90&&word(r,0x2268)!=0&&((word(r,0x2268)-1)&1u)==0)
        for(unsigned i:{0u,1u,2u,4u,5u,6u,8u,9u,10u})check(std::isfinite(std::bit_cast<float>(word(r,0x7800+i*4))),"launch corpus outside finite domain");
    for(unsigned i=0;i<96;++i)for(unsigned offset:{0u,4u,8u,32u,36u,40u,48u,52u,56u})check(std::isfinite(std::bit_cast<float>(word(r,0x11000+i*80+offset))),"launch corpus outside finite domain");
    for(unsigned i=0;i<256;++i)check(std::isfinite(std::bit_cast<float>(word(r,0x14800+i*16+4))),"launch corpus outside finite domain");
}
}
