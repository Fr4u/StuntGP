#pragma once
#include "sgn/vehicle_tick.hpp"
#include <bit>
#include <cstring>
namespace sgn::retail::tick_oracle {
template<class Record>void transfer(Record& b,VehicleTickState& s,bool read) {
    auto field=[&](std::uint32_t& value,unsigned offset) {
        if(read)std::memcpy(&value,b.data()+offset,4);else std::memcpy(b.data()+offset,&value,4);
    };
    auto gap=std::bit_cast<std::uint32_t>(s.performance_gap);field(gap,0x2638);
    if(read)s.performance_gap=std::bit_cast<float>(gap);
    field(s.start_boost_ticks,0x76fc);field(s.low_energy_phase,0x2578);field(s.low_energy_notice,0x7704);
}
}
