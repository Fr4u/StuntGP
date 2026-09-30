#pragma once
#include "sgn/vehicle_ai_automatic.hpp"
#include "vehicle_oracle_state.hpp"
#include "retail_race_state.hpp"

namespace sgn::retail::automatic_oracle {
template<class Record>void transfer(Record& b,AiAutomaticMemory& s,bool read) {
    auto u=[&](std::uint32_t& value,std::size_t at){if(read)std::memcpy(&value,b.data()+at,4);else std::memcpy(b.data()+at,&value,4);};
    auto i=[&](std::int32_t& value,std::size_t at){auto bits=std::bit_cast<std::uint32_t>(value);u(bits,at);if(read)value=std::bit_cast<std::int32_t>(bits);};
    auto f=[&](float& value,std::size_t at){auto bits=std::bit_cast<std::uint32_t>(value);u(bits,at);if(read)value=std::bit_cast<float>(bits);};
    i(s.reverse_ticks,0x2664);f(s.bend,0x2668);f(s.lateral_fraction,0x266c);u(s.counter2674,0x2674);
    i(s.passing_ticks,0x268c);u(s.counter2694,0x2694);i(s.following_ticks,0x26a0);i(s.steering_ramp_ticks,0x26a4);
    u(s.start_throttle_ticks,0x26ac);
}
}
