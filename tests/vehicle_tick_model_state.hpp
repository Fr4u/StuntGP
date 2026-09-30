#pragma once
#include "vehicle_tick_visual_state.hpp"

namespace sgn::retail::tick_test {
// Imports only the original INITIAL mutable model state before a native tick
// sequence. Never call between continuous ticks to import expected answers.
// authored is the unrelocated immutable PMD used to construct ModelResource;
// source_prestate is the same-size original relocated PMD after initialization.
// Pointer-bearing descriptor/part fields are never copied into native storage.
inline void apply_model_visual(ModelResource& model,std::span<const std::uint8_t> authored,
                               std::span<const std::uint8_t> source_prestate){
    if(source_prestate.size()!=authored.size())throw FormatError("model prestate length differs from authored PMD");
    // Validate exactly the same source domain and aliases as after-state capture.
    static_cast<void>(capture_model_visual(model,authored));
    auto word=[&](std::size_t at){if(at>authored.size()||authored.size()-at<4)throw FormatError("model prestate PMD field bounds");
        std::uint32_t value;std::memcpy(&value,authored.data()+at,4);return value;};
    struct Write {std::size_t offset;std::uint8_t value;};
    std::map<std::uint8_t*,Write> staged;
    auto stage=[&](void* target,std::size_t at,std::size_t size){
        if(at>source_prestate.size()||size>source_prestate.size()-at)throw FormatError("model prestate span outside PMD");
        auto* destination=static_cast<std::uint8_t*>(target);
        for(std::size_t i=0;i<size;++i){const auto [found,added]=staged.emplace(destination+i,Write{at+i,source_prestate[at+i]});
            if(!added&&(found->second.offset!=at+i||found->second.value!=source_prestate[at+i]))throw FormatError("model prestate alias identity mismatch");}
    };
    const std::size_t descriptor=328ull+word(0x4c),part_base=328ull+word(0x48),transform_base=328ull+word(0x44);
    const std::size_t attribute_base=328ull+word(0x24),coordinate_base=328ull+word(0x28);const auto transforms=word(0xd8)/80;
    for(unsigned lod=0;lod<model.lod_count();++lod){const auto first=word(descriptor+0x48+lod*4);
        for(unsigned i=0;i<model.part_count(lod);++i){const auto at=part_base+(std::size_t(first)+i)*64;auto part=model.source_part(lod,i);
            stage(&part.flags,at,2);stage(&model.source_part_flags_02(lod,i),at+2,2);
            const auto first_transform=word(at+8);if(first_transform>transforms)throw FormatError("model prestate transform index");
            const auto run=model.source_transform_run(lod,i,transforms-first_transform);stage(run.data(),transform_base+std::size_t(first_transform)*80,run.size_bytes());
            const auto attributes=model.source_attributes(lod,i);stage(attributes.data(),attribute_base+std::size_t(word(at+0x34))*8,attributes.size_bytes());
            const auto coordinates=model.source_coordinates(lod,i);stage(coordinates.data(),coordinate_base+std::size_t(word(at+0x2c))*16,coordinates.size_bytes());
        }
    }
    // All lengths, offsets and overlapping aliases are validated before writing.
    for(const auto& [destination,value]:staged)*destination=value.value;
}
}
