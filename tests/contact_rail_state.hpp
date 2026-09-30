#pragma once
#include "sgn/contact_rail_state.hpp"
#include <bit>
#include <cstring>
namespace sgn::retail::contact::rail_oracle {
template<class Record>
void transfer(Record& b,VehicleRailState& state,bool read){
    const auto u=[&](std::uint32_t& v,unsigned at){if(read)std::memcpy(&v,b.data()+at,4);else std::memcpy(b.data()+at,&v,4);};
    const auto f=[&](float& v,unsigned at){auto bits=std::bit_cast<std::uint32_t>(v);u(bits,at);if(read)v=std::bit_cast<float>(bits);};
    f(state.effect_position.x,0x327c);f(state.effect_position.y,0x3280);f(state.effect_position.z,0x3284);u(state.effect_position.opaque,0x3288);
    u(state.progress,0x7650);f(state.projected_position.x,0x7664);f(state.projected_position.y,0x7668);f(state.projected_position.z,0x766c);
}
}
