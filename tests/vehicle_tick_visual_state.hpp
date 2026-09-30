#pragma once
#include "sgn/retail_vehicle_storage.hpp"
#include <bit>
#include <cstring>
#include <map>

namespace sgn::retail::tick_test {
// Full raw vehicle offsets. Common physics/parameters/race/contact and authored
// anchors deliberately remain in their existing mappers. In particular the
// eight camera-effect counters are mapped by vehicle_oracle_state.hpp.
// decode/encode MUST translate the fixture's guest model identities; no host
// pointer, guessed sentinel or identity convention is imposed here. Audio
// handles are retail driver slot indices, not guest device pointers.
template<class DecodeModel,class EncodeModel>
void transfer_visual(std::span<std::uint8_t> raw,VehicleStorage& storage,unsigned slot,bool loading,
                     DecodeModel decode,EncodeModel encode){
    if(raw.size()<0x7724||slot>=VehicleStorage::physical_slots)throw FormatError("visual vehicle mapping bounds");
    auto blob=[&](auto& value,std::size_t at){
        if(loading)std::memcpy(&value,raw.data()+at,sizeof(value));else std::memcpy(raw.data()+at,&value,sizeof(value));
    };
    auto handle=[&](std::optional<std::size_t>& value,std::size_t at){
        std::uint32_t word{};if(loading){std::memcpy(&word,raw.data()+at,4);value=decode(word);}
        else {word=encode(value);std::memcpy(raw.data()+at,&word,4);}
    };
    auto& p=storage.presentation[slot];auto& model=storage.models[slot];auto& wheels=storage.wheels[slot];
    blob(p.field_3134,0x3134);blob(p.field_33c8,0x33c8);blob(p.field_3508,0x3508);
    blob(p.field_3924,0x3924);blob(p.field_3928,0x3928);blob(p.idle_hold,0x7614);blob(p.idle_interval,0x7618);
    handle(model.model,0x26b0);handle(model.shadow,0x26b4);
    blob(model.position,0x26b8);blob(model.scale,0x26c8);blob(model.target,0x26d8);blob(model.up,0x26e8);
    blob(model.template_tail,0x26f8);blob(model.field_2708,0x2708);blob(model.body_rotation,0x270c);
    for(unsigned i=0;i<4;++i){blob(p.wheel_centers[i],0x201c+i*0x98);
        blob(wheels[i].previous_angle,0x2070+i*0x98);blob(wheels[i].current_angle,0x2074+i*0x98);blob(wheels[i].rotation,0x274c+i*64);}
    auto& trails=storage.trails[slot];auto& inputs=storage.trail_inputs[slot];
    blob(inputs.field_28a0,0x28a0);blob(trails.speed,0x351c);
    constexpr std::array<std::size_t,2> a{0x2d14,0x2f2c},b{0x28d0,0x2aec};
    constexpr std::array<std::size_t,2> ai{0x2d04,0x2f1c},bi{0x28c0,0x2adc};
    constexpr std::array<std::size_t,2> ac{0x2f14,0x312c},bc{0x2ad0,0x2cec},bf{0x28bc,0x2ad8};
    for(unsigned i=0;i<2;++i){blob(inputs.pair_a[i],ai[i]);blob(inputs.pair_b[i],bi[i]);blob(inputs.pair_b_available[i],bf[i]);
        blob(trails.pair_a[i],a[i]);blob(trails.pair_b[i],b[i]);blob(trails.cursor_a[i],ac[i]);blob(trails.cursor_b[i],bc[i]);}
    static_assert(sizeof(VehicleSegment)==48 && sizeof(VehicleSegments)==288);
    blob(storage.segments[slot].items,0x315c);
    auto& effects=storage.model_effects[slot];blob(effects.phase,0x225c);blob(effects.enabled,0x226c);
    blob(effects.cached_brake,0x28a4);blob(effects.route_envelope,0x7684);
    for(unsigned i=0;i<2;++i){blob(effects.current_scale[i],0x32b0+i*0x140);blob(effects.trails[i],0x32c0+i*0x140);blob(effects.cursor[i],0x33c0+i*0x140);}
    auto& shadow=storage.shadows[slot];blob(shadow.factor_392c,0x392c);blob(shadow.factor_3930,0x3930);
    blob(shadow.position,0x3934);blob(shadow.normal,0x3944);blob(shadow.forward,0x3954);
    auto& audio=storage.audio[slot];blob(audio.horn_handle,0x2338);blob(audio.fields_233c,0x233c);
    blob(audio.effect_hold,0x234c);blob(audio.field_2350,0x2350);blob(audio.effect_pending,0x2354);blob(audio.field_2358,0x2358);blob(audio.effect_strength,0x235c);
    blob(audio.impact_handle,0x2370);blob(audio.previous_wheel_touched,0x2374);
    blob(audio.grip_handle,0x237c);blob(audio.scrape_handle,0x2380);blob(audio.rolling_handle,0x2384);blob(audio.boost_handle,0x2388);
    blob(audio.idle_handle,0x238c);blob(audio.rev_handle,0x2390);blob(audio.motion_handle,0x239c);
    blob(audio.field_7718,0x7718);blob(audio.field_771c,0x771c);blob(audio.field_7720,0x7720);
}

// Captures mutable ModelResource words at OFFSETS INTO THE ORIGINAL PMD FILE.
// Compare these directly with original relocated-PMD bytes at the same offsets;
// these regions contain no relocated descriptor/part pointers. This is a sparse
// visual-state capture, not a claim to compare every PMD byte. Aliased spans and
// the contiguous transform tails used by413E60 are captured from their live owners.
inline std::map<std::size_t,std::uint32_t> capture_model_visual(ModelResource& model,std::span<const std::uint8_t> authored){
    auto word=[&](std::size_t at){if(at>authored.size()||authored.size()-at<4)throw FormatError("model visual PMD mapping bounds");
        std::uint32_t value;std::memcpy(&value,authored.data()+at,4);return value;};
    if(authored.size()<328||std::string_view(reinterpret_cast<const char*>(authored.data()),9)!="PMD V1.83")throw FormatError("model visual capture requires PMD V1.83");
    const std::size_t descriptor=328ull+word(0x4c),part_base=328ull+word(0x48),transform_base=328ull+word(0x44);
    const std::size_t attribute_base=328ull+word(0x24),coordinate_base=328ull+word(0x28);const auto transforms=word(0xd8)/80;
    std::map<std::size_t,std::uint32_t> out;
    auto capture=[&](std::size_t at,const void* value,std::size_t bytes){
        if(at>authored.size()||bytes>authored.size()-at||bytes%4)throw FormatError("model visual capture span bounds");
        const auto* data=static_cast<const std::uint8_t*>(value);
        for(std::size_t i=0;i<bytes;i+=4){std::uint32_t bits;std::memcpy(&bits,data+i,4);const auto [found,added]=out.emplace(at+i,bits);
            if(!added&&found->second!=bits)throw FormatError("aliased model visual words disagree");}
    };
    for(unsigned lod=0;lod<model.lod_count();++lod){const auto first=word(descriptor+0x48+lod*4);
        for(unsigned i=0;i<model.part_count(lod);++i){const auto at=part_base+(std::size_t(first)+i)*64;auto bound=model.source_part(lod,i);
            const std::uint32_t flags=bound.flags|(std::uint32_t(model.source_part_flags_02(lod,i))<<16);capture(at,&flags,4);
            const auto first_transform=word(at+8);if(first_transform>transforms)throw FormatError("model visual transform bounds");
            const auto run=model.source_transform_run(lod,i,transforms-first_transform);capture(transform_base+std::size_t(first_transform)*80,run.data(),run.size_bytes());
            const auto attrs=model.source_attributes(lod,i);capture(attribute_base+std::size_t(word(at+0x34))*8,attrs.data(),attrs.size_bytes());
            const auto coords=model.source_coordinates(lod,i);capture(coordinate_base+std::size_t(word(at+0x2c))*16,coords.data(),coords.size_bytes());
        }
    }
    return out;
}
}
