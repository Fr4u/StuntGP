#pragma once
#include "source_pmd_brake_lights_fixture.hpp"
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
namespace sgn::brake_effects_test {
using namespace retail;
using brake_test::check;using brake_test::rejects;using brake_test::Fixture;using brake_test::snapshot;
using Record=std::array<std::uint8_t,0x16000>;
inline unsigned word(const Record& b,unsigned at){check(at<=b.size()-4,"brake effects word bounds");unsigned v;std::memcpy(&v,b.data()+at,4);return v;}
inline void word(Record& b,unsigned at,unsigned v){check(at<=b.size()-4,"brake effects store bounds");std::memcpy(b.data()+at,&v,4);}
inline std::uint16_t half(const Record& b,unsigned at){std::uint16_t v;std::memcpy(&v,b.data()+at,2);return v;}
inline void half(Record& b,unsigned at,std::uint16_t v){std::memcpy(b.data()+at,&v,2);}
inline void strengths(Record& b,VehicleState& state,bool reading){
    for(unsigned i=0;i<4;++i){auto& value=state.wheels[i].braking_signal;if(reading)value=std::bit_cast<float>(word(b,0x2cf4+i*4));else word(b,0x2cf4+i*4,std::bit_cast<unsigned>(value));}
}
inline std::array<unsigned,4> strengths(const VehicleState& state){
    std::array<unsigned,4> out{};for(unsigned i=0;i<4;++i)out[i]=std::bit_cast<unsigned>(state.wheels[i].braking_signal);return out;
}
inline Bytes snapshot(const VehicleState& s){Bytes bytes(sizeof(s));std::memcpy(bytes.data(),&s,sizeof(s));return bytes;}
inline void check_vehicle(const VehicleState& s,const Bytes& before){
    const auto after=snapshot(s);const auto start=reinterpret_cast<std::uintptr_t>(&s);std::set<unsigned> allowed;
    for(const auto& wheel:s.wheels){const auto at=static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(&wheel.braking_signal)-start);for(unsigned i=0;i<4;++i)allowed.insert(at+i);}
    for(unsigned i=0;i<after.size();++i)if(!allowed.contains(i))check(before[i]==after[i],"brake effects wrote unrelated vehicle byte");
}
inline std::set<unsigned> footprint(SourcePmdModelView model,const VehicleState& state){
    std::set<unsigned> allowed;auto range=[&](unsigned at,unsigned n){for(unsigned i=0;i<n;++i)allowed.insert(at+i);};
    for(unsigned raw=0;raw<3;++raw)range(model.part(model.source_lod_index(raw),6).location().offset,2);
    const auto lod=model.source_lod_index(model.current_detail_level().value());
    for(unsigned i=0;i<4;++i){const auto value=state.wheels[i].braking_signal;if(!(value<0||value>0))continue;
        const auto part=model.part(lod,6+i);range(part.location().offset,2);range(part.transform().location().offset+32,32);
        for(unsigned j=0;j<part.attribute_count().value();++j){range(part.attribute_byte(j,2).location().offset,1);range(part.attribute_byte(j,3).location().offset,1);}
    }
    return allowed;
}
inline unsigned section(SourcePmdModelView model,unsigned header){return 0x148+model.bytes().field<unsigned>(header).value();}
inline void pointer(SourcePmdModelView model,SourcePmdPartView part,unsigned member,unsigned value){model.bytes().field<unsigned>(part.location().offset+member).set(value);}
struct Projection {
    Fixture owner;std::array<unsigned,4> heads{};std::map<unsigned,unsigned> transforms;unsigned attributes{};
    Projection(const std::filesystem::path& root,const Record& b):owner(root){
        check(word(b,0x7e0c)==0,"invalid brake effects step");const auto raw=word(b,0x7e08);check(raw<4,"invalid brake effects LOD");
        std::array<unsigned,4> mapping{};for(unsigned i=0;i<4;++i){mapping[i]=word(b,0x7e20+i*4);check(mapping[i]<4,"invalid brake effects raw map");}
        const auto model=owner.view();const auto bytes=model.bytes();const auto descriptor=model.descriptor();
        const auto first=descriptor.field<unsigned>(0x48).value(),second=descriptor.field<unsigned>(0x4c).value();
        check(first!=second,"car1 fixture needs two physical part ranges");
        heads={first,first+12*64,second,second+12*64};
        //Four ten-part synthetic windows use two actual relocated car1 ranges.
        for(unsigned group=0;group<4;++group)for(unsigned i=6;i<=9;++i)bytes.field<std::uint16_t>(heads[group]+i*64).set(half(b,0x13000+(group*24+i)*2));
        for(unsigned slot=0;slot<4;++slot)descriptor.field<unsigned>(0x48+slot*4).set(heads[mapping[slot]]);
        descriptor.field<unsigned>(0x58).set(3);model.current_detail_level().set(raw);descriptor.field<unsigned>(0x24).set(word(b,0x7e10));
        const auto lod=model.source_lod_index(raw),group=mapping[raw],transform_start=section(model,0x44);
        for(unsigned i:{1u,2u,3u,4u,6u,7u,8u,9u}){
            const auto id=word(b,0x13400+(group*24+i)*16);check(id<96,"invalid brake effects transform identity");
            if(!transforms.contains(id)){const auto at=transform_start+static_cast<unsigned>(transforms.size())*80;transforms[id]=at;
                SourcePmdTransform value;std::memcpy(&value,b.data()+0x11000+id*80,80);bytes.field<SourcePmdTransform>(at).set(value);}
            pointer(model,model.part(lod,i),8,transforms.at(id));
        }
        attributes=section(model,0x24);
        for(unsigned i=0;i<128;++i){std::array<std::uint8_t,8> value;std::memcpy(value.data(),b.data()+0x14000+i*8,8);bytes.field<std::array<std::uint8_t,8>>(attributes+i*8).set(value);}
        for(unsigned i=6;i<=9;++i){const auto meta=word(b,0x13400+(group*24+i)*16+4);const auto start=meta&0xffff,count=meta>>16;
            check(start+count<=128,"invalid brake effects attribute span");const auto part=model.part(lod,i);part.attribute_count().set(static_cast<std::uint16_t>(count));pointer(model,part,0x34,attributes+start*8);}
    }
    void pack(Record& b){const auto bytes=owner.view().bytes();
        for(unsigned group=0;group<4;++group)for(unsigned i=6;i<=9;++i)half(b,0x13000+(group*24+i)*2,bytes.field<std::uint16_t>(heads[group]+i*64).value());
        for(const auto [id,at]:transforms){const auto value=bytes.field<SourcePmdTransform>(at).value();std::memcpy(b.data()+0x11000+id*80,&value,80);}
        for(unsigned i=0;i<128;++i){const auto value=bytes.field<std::array<std::uint8_t,8>>(attributes+i*8).value();std::memcpy(b.data()+0x14000+i*8,value.data(),8);}
    }
};
}
