#pragma once
#include "sgn/retail_vehicle_load.hpp"
#include <cstring>
namespace sgn::retail::load_oracle {
inline void transfer(std::span<std::uint8_t> b,VehiclePositionHistory& history,VehicleLoadState& state,bool read) {
    auto field=[&](auto& value,std::size_t at){if(read)std::memcpy(&value,b.data()+at,sizeof(value));else std::memcpy(b.data()+at,&value,sizeof(value));};
    for(unsigned i=0;i<8;++i){auto& sample=history.samples[i];const auto at=0x1f2c+i*0x14;
        field(sample.position.x,at);field(sample.position.y,at+4);field(sample.position.z,at+8);field(sample.position.w,at+12);field(sample.radius,at+16);}
    field(history.ticks,0x1fcc);field(state.current_refuel_rate,0x2274);
    field(state.sounds.horn,0x7708);field(state.sounds.engine_rev,0x770c);field(state.sounds.engine_idle,0x7710);field(state.sounds.engine_boost,0x7714);
}
}
