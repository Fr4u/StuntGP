#pragma once
#include "source_pmd_brake_lights_fixture.hpp"
#include "sgn/vehicle_model_detail.hpp"
#include <bit>
#include <cstring>
#include <fstream>
namespace sgn::presentation_test {
using namespace retail;using sgn::brake_test::check;using sgn::brake_test::rejects;using sgn::brake_test::Fixture;using sgn::brake_test::snapshot;
constexpr unsigned record_size=0xb000,model_at=0x7800,parts_at=0x7900,transforms_at=0x9900;
using Record=std::array<std::uint8_t,record_size>;
inline unsigned word(const Record& b,unsigned at){check(at<=record_size-4,"presentation word bounds");unsigned v;std::memcpy(&v,b.data()+at,4);return v;}
inline void word(Record& b,unsigned at,unsigned v){check(at<=record_size-4,"presentation store bounds");std::memcpy(b.data()+at,&v,4);}
inline Vec3 vector(const Record& b,unsigned at){return {std::bit_cast<float>(word(b,at)),std::bit_cast<float>(word(b,at+4)),std::bit_cast<float>(word(b,at+8))};}
inline VehiclePresentationEffects effects(const Record& b){return {word(b,0x226c),word(b,0x328c),word(b,0x33cc),vector(b,0x32b0),vector(b,0x33f0)};}
inline void state(Record& b,VehiclePresentationState& s,bool reading){
    auto value=[&](unsigned& v,unsigned at){if(reading)v=word(b,at);else word(b,at,v);};
    value(s.field_3134,0x3134);value(s.field_33c8,0x33c8);value(s.field_3508,0x3508);value(s.field_3924,0x3924);value(s.field_3928,0x3928);
}
inline std::array<unsigned,5> fields(const VehiclePresentationState& s){return {s.field_3134,s.field_33c8,s.field_3508,s.field_3924,s.field_3928};}
inline std::set<unsigned> footprint(SourcePmdModelView m,unsigned lod,const VehiclePresentationEffects& e){
    std::set<unsigned> a;auto flag=[&](unsigned i){const auto p=m.part(lod,i).location().offset;a.insert(p);a.insert(p+1);};
    auto scale=[&](unsigned i){const auto at=m.part(lod,i).transform().location().offset+16;for(unsigned j=0;j<12;++j)a.insert(at+j);};
    for(unsigned i=11;i<=16;++i)flag(i);
    if(e.field_226c==0){if(e.field_328c==1)flag(19);if(e.field_33cc==1)flag(20);}
    else if(e.field_328c==1){flag(19);scale(19);if(e.field_33cc==1){flag(20);scale(20);}}
    return a;
}
inline std::size_t unchanged(const Bytes& a,const Bytes& b,const std::set<unsigned>& allowed){
    check(a.size()==b.size(),"presentation resized PMD");std::size_t n{};
    for(unsigned i=0;i<a.size();++i)if(!allowed.contains(i)){check(a[i]==b[i],"presentation PMD write outside exact footprint");++n;}return n;
}
struct Projection {
    Fixture owner;unsigned selected{},lod{},base{},first_id{},second_id{};std::array<unsigned,2> transforms{};
    Projection(const std::filesystem::path& root,const Record& b):owner(root){
        auto m=owner.view();selected=word(b,model_at+0x5c);check(selected<4,"invalid standalone presentation LOD");
        m.current_detail_level().set(selected);lod=m.source_lod_index(selected);base=parts_at+selected*32*64;
        for(unsigned i:{11u,12u,13u,14u,15u,16u,19u,20u}){auto p=m.part(lod,i);const auto flags=word(b,base+i*64);p.flags().set(static_cast<std::uint16_t>(flags));p.flags_02().set(static_cast<std::uint16_t>(flags>>16));}
        first_id=word(b,base+19*64+8);second_id=word(b,base+20*64+8);check(first_id<32&&second_id<32,"invalid presentation transform identity");
        auto a=m.part(lod,19),c=m.part(lod,20);transforms={a.transform().location().offset,c.transform().location().offset};check(transforms[0]!=transforms[1],"car1 fixture needs two real transforms");
        SourcePmdTransform first;std::memcpy(&first,b.data()+transforms_at+first_id*80,80);a.transform().set(first);
        if(first_id==second_id){m.bytes().field<unsigned>(c.location().offset+8).set(transforms[0]);transforms[1]=transforms[0];}
        else{SourcePmdTransform second;std::memcpy(&second,b.data()+transforms_at+second_id*80,80);c.transform().set(second);}
    }
    void pack(Record& b){auto m=owner.view();
        for(unsigned i:{11u,12u,13u,14u,15u,16u,19u,20u})word(b,base+i*64,(word(b,base+i*64)&0xffff0000u)|m.part(lod,i).flags().value());
        const auto a=m.part(lod,19).transform().value();std::memcpy(b.data()+transforms_at+first_id*80,&a,80);
        const auto c=m.part(lod,20).transform().value();std::memcpy(b.data()+transforms_at+second_id*80,&c,80);
    }
};
}
