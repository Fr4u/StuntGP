#pragma once
#include "retail_camera_state.hpp"
#include "sgn/retail_camera_lifecycle.hpp"
namespace sgn::retail::camera_test {
inline void lifecycle(Bytes& c,CameraLifecycle& life,bool reading) {
    auto integer=[&](std::uint32_t& x,std::size_t at) {if(reading)x=word(c,at);else put(c,at,x);};
    auto scalar=[&](float& x,std::size_t at) {if(reading)x=real(c,at);else put(c,at,std::bit_cast<std::uint32_t>(x));};
    auto index=[&](std::optional<std::uint32_t>& x,std::size_t at) {
        if(reading) {const auto value=word(c,at);x=value?std::optional(value-1):std::nullopt;}
        else put(c,at,x?*x+1:0);
    };
    scalar(life.scale.x,0x18);scalar(life.scale.y,0x1c);scalar(life.scale.z,0x20);scalar(life.scale.w,0x24);
    integer(life.transform_word_48,0x48);integer(life.transform_word,0x54);scalar(life.clip_near,0x198);scalar(life.clip_far,0x19c);scalar(life.base_lens,0x1a0);
    integer(life.unknown_1c8,0x1c8);integer(life.next_mode,0x1d0);integer(life.pending_mode,0x1dc);
    index(life.current_record,0x1cc);index(life.pending_record,0x1d8);
}
}
