#pragma once
#include "sgn/retail_vehicle_anchors.hpp"
#include <bit>
#include <cstring>
namespace sgn::retail::anchor_oracle {
template<class Record> void transfer(Record& b,VehicleAnchorState& a,VehiclePresentationState& p,
                                      VehicleTrailInputs& t,VehicleRaceState& r,bool read){
    if(b.size()<0x7800)throw std::runtime_error("incomplete anchor vehicle image");
    auto u=[&](std::uint32_t& value,std::size_t at){if(read)std::memcpy(&value,b.data()+at,4);else std::memcpy(b.data()+at,&value,4);};
    auto f=[&](float& value,std::size_t at){auto bits=std::bit_cast<std::uint32_t>(value);u(bits,at);if(read)value=std::bit_cast<float>(bits);};
    auto v=[&](VehicleVector& value,std::size_t at){f(value.x,at);f(value.y,at+4);f(value.z,at+8);f(value.w,at+12);};
    auto active=std::bit_cast<std::uint32_t>(r.active);u(active,0x1ba8);if(read)r.active=std::bit_cast<std::int32_t>(active);
    u(p.field_3134,0x3134);v(a.position,0x3138);v(a.forward,0x3148);f(a.extent,0x3158);
    u(a.effect_available[0],0x328c);u(a.effect_available[1],0x33cc);
    v(a.effect_position[0],0x3290);v(a.effect_position[1],0x33d0);v(a.effect_base_scale[0],0x32a0);v(a.effect_base_scale[1],0x33e0);
    u(t.field_28a0,0x28a0);v(t.pair_a[0],0x2d04);v(t.pair_a[1],0x2f1c);v(t.pair_b[0],0x28c0);v(t.pair_b[1],0x2adc);
    u(t.pair_b_available[0],0x28bc);u(t.pair_b_available[1],0x2ad8);
}
}
