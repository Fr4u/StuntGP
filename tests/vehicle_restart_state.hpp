#pragma once
#include "sgn/vehicle_restart.hpp"
#include <cstring>

namespace sgn::retail::restart_oracle {
template<class Record> void transfer(Record& b,VehicleRestartState& state,bool read) {
    if(b.size()<0x7800)throw std::runtime_error("restart fixture size");
    const auto byte=[&](std::uint8_t& value,unsigned at){if(read)value=b[at];else b[at]=value;};
    const auto word=[&](std::uint32_t& value,unsigned at){if(read)std::memcpy(&value,b.data()+at,4);else std::memcpy(b.data()+at,&value,4);};
    auto& previous=state.previous_input;
    byte(previous.throttle,0x22a0);byte(previous.brake,0x22a1);byte(previous.left,0x22a2);byte(previous.right,0x22a3);
    byte(previous.rear_lock,0x22a6);byte(previous.steering_boost,0x22a7);byte(previous.turbo,0x22a8);byte(previous.auxiliary,0x22a9);
    word(state.mode_2620,0x2620);word(state.mode_2630,0x2630);
}
}
