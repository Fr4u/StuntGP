#pragma once
#include "vehicle_model_prepare_full_state.hpp"

namespace sgn::retail::prepare_full_test {
// Receiving only. Input values come from the existing original-code corpus;
// relocation identities come from the current canonical loader's allocation.
struct CanonicalModelLayout:ModelLayout {
    using ModelLayout::ModelLayout;
    SourcePmdArrayView<SourcePmdTransform> run(SourcePmdModelView model) const{
        auto result=model.part(model.source_lod_index(0),0).transform_run(transform_count);
        require(result.location().offset==transform_base,"canonical transform table identity");return result;
    }
    Table table(SourcePmdModelView model) const{
        Table result;const auto values=run(model);
        for(unsigned i=0;i<transform_count;++i)result[i]=std::bit_cast<ModelTransform>(values.at(i).value());return result;
    }
    std::uint64_t verify_aliases(SourcePmdModelView model) const{
        std::uint64_t checks=0;
        for(unsigned lod=0;lod<4;++lod){const auto selected=model.source_lod_index(lod);
            for(unsigned i=0;i<25;++i){const auto p=model.part(selected,i);
                require(p.transform().location().offset==transform_base+transform_index[lod][i]*80,"canonical authored transform alias mismatch");++checks;}
            for(unsigned other=0;other<lod;++other)if(first_part[lod]==first_part[other]){
                require(model.part(selected,0).location()==model.part(model.source_lod_index(other),0).location(),"canonical raw LOD alias mismatch");++checks;}}
        return checks;
    }
    void load_image(SourcePmdModelView model,std::span<const std::uint8_t> authored,std::span<const std::uint8_t> live) const{
        require(authored.size()==live.size(),"prepare relocated PMD size");
        auto value=[&]<class T>(unsigned at){require(at<=live.size()&&sizeof(T)<=live.size()-at,"prepare image field bounds");T out;std::memcpy(&out,live.data()+at,sizeof(T));return out;};
        const auto transforms=run(model);
        for(unsigned i=0;i<transform_count;++i)transforms.at(i).set(value.template operator()<SourcePmdTransform>(transform_base+i*80));
        model.current_detail_level().set(word(live,descriptor+0x5c));model.previous_detail_level().set(word(live,descriptor+0x64));
        model.descriptor().field<std::uint16_t>(2).set(static_cast<std::uint16_t>(word(live,descriptor)>>16));
        model.descriptor().field<std::uint16_t>(4).set(static_cast<std::uint16_t>(word(live,descriptor+4)));
        model.descriptor().field<VehicleVector>(0x10).set(value.template operator()<VehicleVector>(descriptor+0x10));
        for(unsigned lod=0;lod<4;++lod){
            require(word(live,descriptor+0x48+lod*4)==base+part_base+first_part[lod]*64,"prepare relocated LOD pointer");
            const auto selected=model.source_lod_index(lod);
            for(unsigned i=0;i<25;++i){const auto at=part_base+(first_part[lod]+i)*64;const auto p=model.part(selected,i);
                require(word(live,at+8)==base+transform_base+transform_index[lod][i]*80,"prepare relocated transform pointer");
                p.flags().set(static_cast<std::uint16_t>(word(live,at)));p.flags_02().set(static_cast<std::uint16_t>(word(live,at)>>16));
                p.word_04().set(static_cast<std::uint16_t>(word(live,at+4)));
                const auto attributes=p.attributes();const auto a=328+word(authored,0x24)+word(authored,at+0x34)*8;
                for(unsigned j=0;j<attributes.size();++j)attributes.at(j).set(value.template operator()<std::array<std::uint8_t,8>>(a+j*8));
                const auto coordinates=p.coordinates();const auto c=328+word(authored,0x28)+word(authored,at+0x2c)*16;
                for(unsigned j=0;j<coordinates.size();++j)coordinates.at(j).set(value.template operator()<std::array<std::uint32_t,4>>(c+j*16));
            }
        }
        (void)verify_aliases(model);
    }
    Bytes immutable(SourceAllocationBank& bank,SourcePmdModelView model) const{
        (void)run(model);const auto bytes=bank.bytes(model.allocation());Bytes result(bytes.begin(),bytes.end());
        require(transform_base<=result.size()&&transform_count*80<=result.size()-transform_base,"canonical immutable mask range");
        result.erase(result.begin()+transform_base,result.begin()+transform_base+transform_count*80);return result;
    }
};
}
