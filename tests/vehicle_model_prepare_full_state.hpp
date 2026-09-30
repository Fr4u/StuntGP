#pragma once
// Reusable actual-PMD / vehicle preparation codec. Test data only; no runtime
// state owner or game behavior is introduced here.
#include "sgn/vehicle_model_prepare.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace sgn::retail::prepare_full_test {
constexpr unsigned vehicle_bytes=0x7724,transform_count=25;
using Car=std::array<std::uint32_t,vehicle_bytes/4>;
using Table=std::array<ModelTransform,transform_count>;
using Mask=std::set<unsigned>;
inline void require(bool value,const char* message){if(!value)throw FormatError(message);}
inline unsigned word(std::span<const std::uint8_t> bytes,std::size_t at){
    require(at<=bytes.size()&&bytes.size()-at>=4,"prepare image word bounds");
    unsigned value;std::memcpy(&value,bytes.data()+at,4);return value;
}
inline bool stimulus_word(unsigned at){
    if(at%4)return false;
    if(at==0x1cf4||at==0x288c||at==0x2890||at==0x2894||at==0x28a0||at==0x3134||at==0x328c||at==0x33cc)return true;
    if(at>=0x274c&&at<0x284c)return true;
    for(unsigned i=0;i<6;++i)if(at>=0x317c+i*48&&at<=0x3188+i*48)return true;
    return false;
}
struct VehicleOwners {
    VehicleState physics{};
    VehicleTrailInputs inputs{};
    std::array<VehicleWheelAnimation,4> wheels{};
    VehiclePresentationState presentation{};
    VehicleAnchorState anchors{};
    VehicleSegments segments{};
    contact::Vector4 effect{}; // sole source327C, matches vehicles.rail[slot].effect_position
    void transfer(Car& raw,bool input){
        auto field=[&](auto& value,unsigned at){
            static_assert(std::is_trivially_copyable_v<std::remove_reference_t<decltype(value)>>);
            if(input)std::memcpy(&value,reinterpret_cast<const std::uint8_t*>(raw.data())+at,sizeof(value));
            else std::memcpy(reinterpret_cast<std::uint8_t*>(raw.data())+at,&value,sizeof(value));
        };
        field(physics.position,0x1bac);field(physics.orientation,0x1bcc);
        field(physics.basis_x,0x1cbc);field(physics.up,0x1ccc);field(physics.basis_y_w,0x1cd8);field(physics.basis_z,0x1cdc);
        field(physics.steering,0x1cf4);field(physics.body_heave,0x288c);field(physics.body_pitch,0x2890);field(physics.body_roll,0x2894);
        field(inputs.field_28a0,0x28a0);field(presentation.field_3134,0x3134);
        field(anchors.position,0x3138);field(anchors.forward,0x3148);field(anchors.extent,0x3158);field(segments,0x315c);
        field(effect,0x327c);
        for(unsigned i=0;i<4;++i)field(wheels[i].rotation,0x274c+i*64);
        for(unsigned i=0;i<2;++i){field(anchors.effect_available[i],0x328c+i*0x140);field(anchors.effect_position[i],0x3290+i*0x140);}
    }
};
struct ModelLayout {
    unsigned base{},descriptor{},part_base{},transform_base{};
    std::array<unsigned,4> first_part{};
    std::array<std::array<unsigned,25>,4> transform_index{};
    explicit ModelLayout(std::span<const std::uint8_t> raw,unsigned address):base(address){
        require(raw.size()>=328&&std::string_view(reinterpret_cast<const char*>(raw.data()),9)=="PMD V1.83","prepare actual PMD signature");
        descriptor=328+word(raw,0x4c);part_base=328+word(raw,0x48);transform_base=328+word(raw,0x44);
        require(word(raw,0xd8)==transform_count*80&&word(raw,descriptor+0x24)==25,"prepare authored model extent");
        for(unsigned lod=0;lod<4;++lod){first_part[lod]=word(raw,descriptor+0x48+lod*4);
            for(unsigned i=0;i<25;++i){const auto index=word(raw,part_base+(first_part[lod]+i)*64+8);
                require(index<transform_count,"prepare authored transform index");transform_index[lod][i]=index;}}
        require(transform_index[0][0]==0,"prepare table first transform");
    }
    std::span<ModelTransform> table(ModelResource& model) const{return model.source_transform_run(0,0,transform_count);}
    unsigned lane(unsigned lod,unsigned part,unsigned offset) const{return 16+transform_index.at(lod).at(part)*20+offset/4;}
    std::uint64_t verify_aliases(ModelResource& model) const{
        const auto transforms=table(model);std::uint64_t checks=0;
        for(unsigned lod=0;lod<4;++lod){const auto selected=model.source_lod_index(lod);
            for(unsigned i=0;i<25;++i){auto p=model.source_part(selected,i);
                require(&p.transform==&transforms[transform_index[lod][i]],"prepare authored transform alias mismatch");++checks;}
            for(unsigned other=0;other<lod;++other)if(first_part[lod]==first_part[other]){
                require(&model.source_part(selected,0).flags==&model.source_part(model.source_lod_index(other),0).flags,"prepare raw LOD part alias mismatch");++checks;}}
        return checks;
    }
    void load_image(ModelResource& model,std::span<const std::uint8_t> authored,std::span<const std::uint8_t> live) const{
        require(authored.size()==live.size(),"prepare relocated PMD size");
        auto copy=[&](auto& value,std::size_t at){require(at<=live.size()&&sizeof(value)<=live.size()-at,"prepare image field bounds");std::memcpy(&value,live.data()+at,sizeof(value));};
        auto transforms=table(model);require(transform_base<=live.size()&&transforms.size_bytes()<=live.size()-transform_base,"prepare image transform bounds");
        std::memcpy(transforms.data(),live.data()+transform_base,transforms.size_bytes());
        model.current_detail_level()=word(live,descriptor+0x5c);model.previous_detail_level()=word(live,descriptor+0x64);
        model.source_model_flags_02()=static_cast<std::uint16_t>(word(live,descriptor)>>16);
        model.source_model_ambience()=static_cast<std::uint16_t>(word(live,descriptor+4));copy(model.source_model_vector_10(),descriptor+0x10);
        for(unsigned lod=0;lod<4;++lod){
            require(word(live,descriptor+0x48+lod*4)==base+part_base+first_part[lod]*64,"prepare relocated LOD pointer");
            const auto selected=model.source_lod_index(lod);
            for(unsigned i=0;i<25;++i){const auto at=part_base+(first_part[lod]+i)*64;auto p=model.source_part(selected,i);
                require(word(live,at+8)==base+transform_base+transform_index[lod][i]*80,"prepare relocated transform pointer");
                p.flags=static_cast<std::uint16_t>(word(live,at));model.source_part_flags_02(selected,i)=static_cast<std::uint16_t>(word(live,at)>>16);
                model.source_part_word_04(selected,i)=static_cast<std::uint16_t>(word(live,at+4));
                auto attributes=model.source_attributes(selected,i);const auto a=328+word(authored,0x24)+word(authored,at+0x34)*8;
                require(a<=live.size()&&attributes.size_bytes()<=live.size()-a,"prepare attribute image bounds");std::memcpy(attributes.data(),live.data()+a,attributes.size_bytes());
                auto coordinates=model.source_coordinates(selected,i);const auto c=328+word(authored,0x28)+word(authored,at+0x2c)*16;
                require(c<=live.size()&&coordinates.size_bytes()<=live.size()-c,"prepare coordinate image bounds");std::memcpy(coordinates.data(),live.data()+c,coordinates.size_bytes());
            }
        }
        (void)verify_aliases(model);
    }
};
inline std::vector<std::uint8_t> immutable_model_state(ModelResource& model){
    std::vector<std::uint8_t> result;
    auto bytes=[&](const void* p,std::size_t size){const auto* begin=static_cast<const std::uint8_t*>(p);if(size)result.insert(result.end(),begin,begin+size);};
    auto field=[&](const auto& value){bytes(&value,sizeof(value));};
    field(model.current_detail_level());field(model.previous_detail_level());field(model.source_model_flags_02());field(model.source_model_ambience());
    field(model.source_model_vector_10());field(model.source_model_bounds());field(model.source_outer_transform());
    bytes(model.source_normals().data(),model.source_normals().size_bytes());bytes(model.source_attribute_table().data(),model.source_attribute_table().size_bytes());
    bytes(model.source_corner_indices().data(),model.source_corner_indices().size_bytes());
    for(unsigned lod=0;lod<model.lod_count();++lod)for(unsigned i=0;i<model.part_count(lod);++i){auto p=model.source_part(lod,i);
        field(p.flags);field(model.source_part_flags_02(lod,i));field(model.source_part_word_04(lod,i));field(p.pivot);field(model.source_part_bounds(lod,i));
        bytes(model.source_coordinates(lod,i).data(),model.source_coordinates(lod,i).size_bytes());bytes(model.source_vertices(lod,i).data(),model.source_vertices(lod,i).size_bytes());}
    return result;
}
// Independently derive only fourth-lane provenance/copy expectations. No XYZ
// arithmetic or native game helper is used to decide a comparison exclusion.
inline std::map<unsigned,unsigned> expected_lanes(const ModelLayout& layout,unsigned lod,const VehicleOwners& v,
    const Table& before,const VehicleMatrix& basis,Mask& taint){
    const auto raw=std::bit_cast<std::array<unsigned,500>>(before);const auto matrix=std::bit_cast<std::array<unsigned,16>>(basis);
    std::map<unsigned,unsigned> expected;
    for(unsigned i=0;i<25;++i)for(unsigned offset:{12u,28u,44u,60u})expected[16+i*20+offset/4]=raw[i*20+offset/4];
    expected[3]=matrix[3];expected[7]=matrix[7];
    for(unsigned p:{0u,5u}){taint.insert(layout.lane(lod,p,44));taint.insert(layout.lane(lod,p,60));}
    if(v.inputs.field_28a0==1){taint.insert(layout.lane(lod,10,44));taint.insert(layout.lane(lod,10,60));}
    for(unsigned i=0;i<2;++i){for(auto [offset,index]:{std::pair{44u,11u},std::pair{60u,7u}}){
        const auto at=layout.lane(lod,i+1,offset);expected[at]=std::bit_cast<unsigned>(v.wheels[i].rotation[index]);taint.erase(at);}}
    for(unsigned p:{3u,4u}){taint.insert(layout.lane(lod,p,44));taint.insert(layout.lane(lod,p,60));}
    taint.insert(3);taint.insert(7);const auto forward=layout.lane(0,0,44);expected[11]=expected.at(forward);
    if(taint.contains(forward))taint.insert(11);else taint.erase(11);
    if(v.presentation.field_3134!=0)for(unsigned i=0;i<6;++i){const auto first=layout.transform_index[lod][11]+i;
        require(first<25,"prepare segment contiguous identity");taint.insert(16+first*20+3);
        const auto at=16+first*20+11;taint.erase(at);expected[at]=std::bit_cast<unsigned>(v.segments.items[i].direction.w);}
    if(v.anchors.effect_available[0]!=0)for(unsigned i=0;i<(v.anchors.effect_available[1]!=0?2u:1u);++i){
        taint.insert(layout.lane(lod,19+i,12));
        for(unsigned offset:{44u,60u}){const auto from=layout.lane(lod,0,offset),to=layout.lane(lod,19+i,offset);expected[to]=expected.at(from);
            if(taint.contains(from))taint.insert(to);else taint.erase(to);}}
    return expected;
}
}
