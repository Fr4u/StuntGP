#pragma once
#include "sgn/vehicle_progression.hpp"
#include <cstring>

namespace sgn::retail::progression_oracle {
template<class Record> void transfer(Record& b,VehicleProgressionState& s,bool read) {
    auto u=[&](auto& value,unsigned at){if(read)std::memcpy(&value,b.data()+at,4);else std::memcpy(b.data()+at,&value,4);};
    u(s.previous_projection,0x23b4);u(s.record_improved,0x23c0);u(s.lap_time_24,0x243c);
    for(unsigned j=0;j<25;++j)u(s.lap_scores[j],0x2440+j*4);
    u(s.checkpoint_index,0x24ec);
    for(unsigned j=0;j<10;++j){u(s.checkpoint_times[j],0x24f0+j*4);u(s.personal_checkpoint_times[j],0x2518+j*4);u(s.record_checkpoint_times[j],0x2540+j*4);}
}
}
