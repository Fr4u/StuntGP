#pragma once
#include "sgn/retail_vehicle_bootstrap.hpp"
#include "contact_solver_state.hpp"
#include "retail_vehicle_load_state.hpp"
#include "retail_vehicle_anchors_state.hpp"
#include "vehicle_restart_state.hpp"
#include "vehicle_tick_state.hpp"
#include <bit>
#include <cstring>
namespace sgn::retail::bootstrap_test {
using Record=std::array<std::uint8_t,0x7800>;
std::uint32_t get(const Record& b,std::size_t at){std::uint32_t value;std::memcpy(&value,b.data()+at,4);return value;}
void put(Record& b,std::size_t at,std::uint32_t value){std::memcpy(b.data()+at,&value,4);}
void transfer(Record& b,VehicleStorage& s,unsigned i,bool read,ModelResources& resources){
    const auto field=[&](auto& value,std::size_t at){if(read)std::memcpy(&value,b.data()+at,sizeof(value));else std::memcpy(b.data()+at,&value,sizeof(value));};
    if(i<6){
        contact::oracle::transfer(b,s.parameters[i],s.physics[i],s.race[i],s.contact[i],read);
        auto span=std::span(b);load_oracle::transfer(span,s.history[i],s.load[i],read);
        anchor_oracle::transfer(b,s.anchors[i],s.presentation[i],s.trail_inputs[i],s.race[i],read);
        restart_oracle::transfer(b,s.restart[i],read);
        tick_oracle::transfer(b,s.tick[i],read);
        field(s.initialization_extras[i].field_28b0,0x28b0);
        auto& startup=s.startup[i];field(startup.field_2280,0x2280);field(startup.fields_768c,0x768c);
        auto& progression=s.progression[i];field(progression.previous_projection,0x23b4);field(progression.record_improved,0x23c0);
        field(progression.lap_time_24,0x243c);field(progression.lap_scores,0x2440);field(progression.checkpoint_index,0x24ec);
        field(progression.checkpoint_times,0x24f0);field(progression.personal_checkpoint_times,0x2518);field(progression.record_checkpoint_times,0x2540);
        field(s.race[i].place,0x24ac);
        if(get(b,0x7268)!=0||get(b,0x726c)!=0)throw std::runtime_error("bootstrap cold fixture has external replay/object binding");
        auto& c=s.camera_settings[i];field(c.orbit_offset,0x22cc);field(c.target_offset,0x22dc);field(c.angles,0x22ec);
        field(c.distance,0x22fc);field(c.approach_scale,0x2300);field(c.adaptive_distance,0x2304);
        for(unsigned j=0;j<4;++j){auto& h=s.strips[i].items[j];field(h.field_00,0x3964+j*0xd10);field(h.field_04,0x3968+j*0xd10);field(h.field_08,0x396c+j*0xd10);}
        // Cold startup leaves these owners zero. They are still emitted so
        // accidental native mutations cannot be hidden by source image reuse.
        auto& t=s.trails[i];field(t.speed,0x351c);
        for(unsigned j=0;j<2;++j){field(t.pair_a[j],0x2d14+j*0x218);field(t.pair_b[j],0x28d0+j*0x21c);field(t.cursor_a[j],0x2f14+j*0x218);field(t.cursor_b[j],0x2ad0+j*0x21c);}
        auto& a=s.audio[i];field(a.horn_handle,0x2338);field(a.fields_233c,0x233c);field(a.effect_hold,0x234c);
        field(a.field_2350,0x2350);field(a.effect_pending,0x2354);field(a.field_2358,0x2358);field(a.effect_strength,0x235c);
        field(a.impact_handle,0x2370);field(a.previous_wheel_touched,0x2374);field(a.grip_handle,0x237c);field(a.scrape_handle,0x2380);
        field(a.rolling_handle,0x2384);field(a.boost_handle,0x2388);field(a.idle_handle,0x238c);field(a.rev_handle,0x2390);field(a.motion_handle,0x239c);
        field(a.field_7718,0x7718);field(a.field_771c,0x771c);field(a.field_7720,0x7720);
        auto& effects=s.model_effects[i];field(effects.phase,0x225c);field(effects.enabled,0x226c);
        field(effects.cached_brake,0x28a4);field(effects.route_envelope,0x7684);
        for(unsigned j=0;j<2;++j){field(effects.current_scale[j],0x32b0+j*0x140);
            field(effects.trails[j],0x32c0+j*0x140);field(effects.cursor[j],0x33c0+j*0x140);}
        auto& particles=s.particles[i];field(particles.field_761c,0x761c);field(particles.field_7620,0x7620);field(particles.field_7628,0x7628);
        for(unsigned j=0;j<4;++j)field(particles.field_1ff8[j],0x1ff8+j*0x98);
        field(s.route_assist[i].blend,0x265c);
        auto& shadow=s.shadows[i];field(shadow.factor_392c,0x392c);field(shadow.factor_3930,0x3930);
        field(shadow.position,0x3934);field(shadow.normal,0x3944);field(shadow.forward,0x3954);
        for(unsigned j=0;j<4;++j){auto& ring=s.skid_rings[i].items[j];const auto at=0x3964+j*0xd10;
            if(read){const auto encoded=get(b,at+12);if(encoded>512)throw std::runtime_error("bootstrap corner identity");
                ring.last_corner=encoded?std::optional(encoded-1):std::nullopt;}
            else put(b,at+12,ring.last_corner?*ring.last_corner+1:0);
            for(unsigned k=0;k<64;++k){auto& segment=ring.segments[k];const auto offset=at+16+k*0x34;
                field(segment.start,offset);field(segment.end,offset+16);field(segment.step,offset+32);field(segment.first_vertex,offset+48);}
        }
    }
    field(s.configuration_identity[i].header,0);
    auto& p=s.presentation[i];field(p.field_3134,0x3134);field(p.field_33c8,0x33c8);field(p.field_3508,0x3508);
    field(p.field_3924,0x3924);field(p.field_3928,0x3928);field(p.idle_hold,0x7614);field(p.idle_interval,0x7618);
    auto& m=s.models[i];
    if(read){m.model=resources.resolve(get(b,0x26b0));m.shadow=resources.resolve(get(b,0x26b4));}
    else{put(b,0x26b0,resources.identity(m.model));put(b,0x26b4,resources.identity(m.shadow));}
    field(m.position,0x26b8);field(m.scale,0x26c8);field(m.target,0x26d8);field(m.up,0x26e8);
    field(m.template_tail,0x26f8);field(m.field_2708,0x2708);field(m.body_rotation,0x270c);
    for(unsigned j=0;j<4;++j){field(p.wheel_centers[j],0x201c+j*0x98);field(s.wheels[i][j].previous_angle,0x2070+j*0x98);
        field(s.wheels[i][j].current_angle,0x2074+j*0x98);field(s.wheels[i][j].rotation,0x274c+j*64);}
}
}
