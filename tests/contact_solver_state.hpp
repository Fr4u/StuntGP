#pragma once
#include "sgn/contact_solver.hpp"
#include "vehicle_oracle_state.hpp"
#include "retail_race_state.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail::contact::oracle {
template<class Record>
void transfer_contact(Record& b,solver::State& c,bool read){
    const auto get=[&](unsigned at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;};
    const auto put=[&](unsigned at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);};
    const auto u=[&](std::uint32_t& v,unsigned at){if(read)v=get(at);else put(at,v);};
    const auto i=[&](std::int32_t& v,unsigned at){if(read)v=std::bit_cast<std::int32_t>(get(at));else put(at,std::bit_cast<std::uint32_t>(v));};
    const auto f=[&](float& v,unsigned at){if(read)v=std::bit_cast<float>(get(at));else put(at,std::bit_cast<std::uint32_t>(v));};
    const auto v=[&](Vector4& x,unsigned at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);u(x.opaque,at+12);};
    v(c.body.position,0x7270);v(c.body.previous_position,0x7280);v(c.body.velocity,0x72a0);f(c.body.mass,0x72b0);f(c.rig.bounding_radius,0x72b4);
    for(unsigned j=0;j<16;++j){f(c.body.orientation[j],0x72b8+j*4);f(c.body.incremental[j],0x72f8+j*4);}
    for(unsigned j=0;j<16;++j)f(c.matrix_7338[j],0x7338+j*4);
    for(unsigned j=0;j<12;++j)f(c.orientation_snapshot[j],0x737c+j*4);
    u(c.rig.count,0x73ac);f(c.rig.origin_y_offset,0x73b0);
    for(unsigned j=0;j<6;++j){auto& q=c.rig.probes[j];const auto at=0x73b4+j*0x4c;
        v(q.local,at);v(q.current,at+16);v(q.previous,at+32);v(q.velocity,at+48);u(q.contact_iterations,at+64);f(q.mass_fraction,at+68);f(q.radius,at+72);
        u(c.pair_counters.current[j],0x25c4+j*4);u(c.pair_counters.previous[j],0x25dc+j*4);u(c.pair_counters.count[j],0x25f4+j*4);
    }
    for(unsigned j=0;j<4;++j){auto& w=c.wheels[j];const auto at=0x1fe0+j*0x98;
        f(w.current_load,at+128);i(w.previous_type,at+116);i(w.type,at+120);i(w.changed,at+124);
    }
    i(c.impact_count.value,0x2568);i(c.impact_count.capacity,0x256c);
    f(c.accumulated_energy_loss,0x257c);i(c.energy_hold,0x2580);f(c.direction_magnitude,0x76f8);f(c.shock,0x2360);f(c.pair_shock,0x2364);i(c.shock_hold,0x236c);i(c.wheel_touched,0x2378);
    i(c.com_reset_ticks,0x2624);u(c.com_2628,0x2628);u(c.com_262c,0x262c);u(c.pair_effect_ticks,0x2670);u(c.effect_2368,0x2368);
    auto& a=c.surface;u(a.saved_controller_mode,0x7630);
    u(a.special_ticks,0x764c);u(a.saved_2278,0x7654);
    for(unsigned j=0;j<3;++j)u(a.special_accumulator[j],0x7658+j*4);
    u(a.saved_24a4,0x7674);
    // Test corpus stores the optional object's VALUE in an unused fixture word.
    // A native runtime uses the optional, never an original pointer.
    if(read){if(get(0x726c))a.object_mode=std::bit_cast<std::int32_t>(get(0x7790));else a.object_mode.reset();}
}
template<class Record>
void transfer(Record& b,VehicleParameters& p,VehicleState& s,VehicleRaceState& race,solver::State& c,bool read){
    sgn::retail::oracle::transfer(b,p,s,read);
    sgn::retail::race_oracle::transfer(b,race,read);
    transfer_contact(b,c,read);
}
}
