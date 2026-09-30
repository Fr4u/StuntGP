#pragma once
// Test-only SGNVPR02 address codec. Guest addresses never enter production
// ownership; the codec translates references at each serialization boundary.
#include "sgn/retail_vehicle_presentation_frame.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace sgn::retail::presentation_test {
inline void require(bool value,const char* message){if(!value)throw FormatError(message);}
struct Region {std::uint32_t kind{},index{},address{},length{},offset{};};
static_assert(sizeof(Region)==20);
struct SourceImage {
    std::vector<Region> regions;
    std::vector<std::uint8_t> bytes;
    std::span<std::uint8_t> at(std::uint32_t address,std::size_t count){
        for(const auto& region:regions)if(address>=region.address&&
            std::uint64_t(address)+count<=std::uint64_t(region.address)+region.length)
            return {bytes.data()+region.offset+address-region.address,count};
        throw FormatError("presentation guest address outside captured owners");
    }
    const Region& region(std::uint32_t kind,std::uint32_t index=0)const{
        for(const auto& value:regions)if(value.kind==kind&&value.index==index)return value;
        throw FormatError("presentation source owner missing");
    }
    std::uint32_t get(std::uint32_t address){std::uint32_t value;auto data=at(address,4);std::memcpy(&value,data.data(),4);return value;}
    void put(std::uint32_t address,std::uint32_t value){auto data=at(address,4);std::memcpy(data.data(),&value,4);}
    template<class T>void field(T& value,std::uint32_t address,bool input){
        static_assert(std::is_trivially_copyable_v<T>);auto data=at(address,sizeof(value));
        if(input)std::memcpy(&value,data.data(),sizeof(value));else std::memcpy(data.data(),&value,sizeof(value));
    }
};
inline constexpr std::uint32_t vehicle_base=0x49a1f8,vehicle_stride=0x7724,matrix_base=0x61c7cc;
inline std::uint32_t matrix_token(std::uint32_t address){
    if(!address)return 0;if(address==0x61c468)return 1;
    require(address>=matrix_base&&address<matrix_base+384*64&&(address-matrix_base)%64==0,"presentation source matrix reference");
    return 2+address-matrix_base;
}
inline std::uint32_t matrix_address(std::uint32_t token){
    if(!token)return 0;if(token==1)return 0x61c468;
    require(token>=2&&(token-2)%64==0&&(token-2)/64<384,"presentation native matrix reference");
    return matrix_base+token-2;
}
template<class Pose>void transfer_pose(SourceImage& image,Pose& pose,std::uint32_t address,bool part,bool input){
    if(input)image.field(pose,address,true);
    auto copy=pose;
    for(auto index:part?std::array{0u,1u}:std::array{1u,2u})
        copy.tail[index]=input?matrix_token(copy.tail[index]):matrix_address(copy.tail[index]);
    if(input)pose=copy;else image.field(copy,address,false);
}

struct ModelImage {
    std::uint32_t base{},descriptor{},part_base{},transform_base{},transforms{},parts{};
    Bytes authored;
    std::array<std::uint32_t,4> first{};
    ModelResource* model{};
    std::uint32_t token{};
    std::uint32_t word(std::size_t at)const{
        require(at<=authored.size()&&authored.size()-at>=4,"presentation authored PMD bounds");
        std::uint32_t value;std::memcpy(&value,authored.data()+at,4);return value;
    }
    ModelImage(std::uint32_t address,Bytes raw,ModelResource& owner,std::uint32_t identity)
        :base(address),authored(std::move(raw)),model(&owner),token(identity){
        require(authored.size()>=328,"presentation PMD header extent");
        descriptor=328+word(0x4c);part_base=328+word(0x48);transform_base=328+word(0x44);
        transforms=word(0xd8)/80;parts=word(descriptor+0x24);
        require(word(0xd8)%80==0&&transforms&&parts,"presentation authored transform extent");
        for(unsigned lod=0;lod<4;++lod)first[lod]=word(descriptor+0x48+lod*4);
        require(word(part_base+first[0]*64+8)==0,"presentation first authored transform");
    }
    void transfer(SourceImage& image,bool input){
        auto table=model->source_transform_run(0,0,transforms);
        for(unsigned i=0;i<transforms;++i)transfer_pose(image,table[i],base+transform_base+i*80,true,input);
        image.field(model->current_detail_level(),base+descriptor+0x5c,input);
        image.field(model->previous_detail_level(),base+descriptor+0x64,input);
        image.field(model->source_model_flags_02(),base+descriptor+2,input);
        image.field(model->source_model_ambience(),base+descriptor+4,input);
        image.field(model->source_model_vector_10(),base+descriptor+0x10,input);
        require(image.get(base+descriptor+12)==0&&!model->source_outer_transform(),"presentation PMD outer binding changed");
        for(unsigned lod=0;lod<4;++lod){
            const auto selected=model->source_lod_index(lod);
            require(model->part_count(selected)==parts,"presentation PMD part count");
            for(unsigned i=0;i<parts;++i){
                const auto at=part_base+(first[lod]+i)*64;auto part=model->source_part(selected,i);
                image.field(part.flags,base+at,input);image.field(model->source_part_flags_02(selected,i),base+at+2,input);
                image.field(model->source_part_word_04(selected,i),base+at+4,input);
                const auto index=word(at+8);require(index<transforms&&&part.transform==&table[index],"presentation authored transform alias");
                auto attributes=model->source_attributes(selected,i);auto coordinates=model->source_coordinates(selected,i);
                auto attrs=image.at(base+328+word(0x24)+word(at+0x34)*8,attributes.size_bytes());
                auto coords=image.at(base+328+word(0x28)+word(at+0x2c)*16,coordinates.size_bytes());
                if(input){std::memcpy(attributes.data(),attrs.data(),attrs.size());std::memcpy(coordinates.data(),coords.data(),coords.size());}
                else {std::memcpy(attrs.data(),attributes.data(),attrs.size());std::memcpy(coords.data(),coordinates.data(),coords.size());}
            }
        }
    }
};
inline std::uint32_t model_token(std::span<const ModelImage> models,std::uint32_t descriptor,
                                std::uint32_t strip,std::uint32_t sprite){
    if(!descriptor)return 0;if(descriptor==strip)return strip_model_identity;if(descriptor==sprite)return sprite_model_identity;
    for(const auto& model:models)if(model.base+model.descriptor==descriptor)return model.token;
    throw FormatError("presentation unknown source model identity");
}
inline std::uint32_t model_address(std::span<const ModelImage> models,std::uint32_t token,
                                  std::uint32_t strip,std::uint32_t sprite){
    if(!token)return 0;if(token==strip_model_identity)return strip;if(token==sprite_model_identity)return sprite;
    for(const auto& model:models)if(model.token==token)return model.base+model.descriptor;
    throw FormatError("presentation unknown native model identity");
}
inline void transfer_queue(SourceImage& image,ModelDrawQueue& queue,std::uint32_t base,std::uint32_t cursor_address,
                           std::span<const ModelImage> models,std::uint32_t strip,std::uint32_t sprite,bool input){
    if(input){const auto cursor=image.get(cursor_address);
        require(cursor>=base&&cursor<=base+167*84&&(cursor-base)%84==0,"presentation queue cursor");queue.cursor=(cursor-base)/84;}
    else {require(queue.cursor&&*queue.cursor<168,"presentation native queue cursor");image.put(cursor_address,base+*queue.cursor*84);}
    for(unsigned i=0;i<168;++i){auto& record=queue.records[i];const auto at=base+i*84;
        if(input)record.model_identity=model_token(models,image.get(at),strip,sprite);
        else image.put(at,model_address(models,record.model_identity,strip,sprite));
        transfer_pose(image,record.pose,at+4,false,input);
    }
}
}
