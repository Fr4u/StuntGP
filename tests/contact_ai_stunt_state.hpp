#pragma once
#include "sgn/contact_ai_stunts.hpp"
#include <bit>
#include <cstring>
namespace sgn::retail::contact::ai_stunt_oracle {
template<class Record>
void transfer(Record& b,AiStuntHelperState& state,bool read){
    const auto u=[&](std::uint32_t& v,unsigned at){if(read)std::memcpy(&v,b.data()+at,4);else std::memcpy(b.data()+at,&v,4);};
    u(state.input_kind,0x267c);u(state.grade,0x2680);
    auto value=std::bit_cast<std::uint32_t>(state.planned_route_distance);u(value,0x2684);if(read)state.planned_route_distance=std::bit_cast<float>(value);
}
}
