#pragma once
// Output-only actual VehicleState owners. No physical update, reset or imported
// fixture initializes these values. VehicleParameters and other bank domains
// are separate owners and never fabricated as padding around these fields.
#include "normal_session_race_export.hpp"

namespace normal_session_owner {
inline void export_vehicle_physics_owners(RaceSession& session,Image& out,NativeAddresses& addresses){
    using namespace race_export_detail;
    const auto& bank=session.state().vehicles.physics;
    static_assert(sizeof(VehicleInput)==10&&sizeof(VehicleStatistic)==16);
    static_assert(sizeof(Vec3)==12&&sizeof(VehicleVector)==16&&sizeof(VehicleMatrix)==64);
    static_assert(std::is_standard_layout_v<VehicleState>&&std::is_trivially_copyable_v<VehicleState>);
    static_assert(sizeof(VehicleState)==2812&&sizeof(Wheel)==76&&sizeof(VehicleStuntState)==1212);
    for(unsigned slot=0;slot<VehicleStorage::physical_slots;++slot){
        const auto base=vehicle_address(slot);const auto key="physics/vehicle/"+std::to_string(slot)+"/";const auto& v=bank[slot];
        const auto emit=[&](const std::string& name,unsigned offset,const auto& value){
            field(out,addresses,key+name,base+offset,value);
            out.alias(key+name+"/actualNative",addresses.address(&value,sizeof(value)),narrow(sizeof(value)));
        };
        const auto live=[&](const char* name,unsigned offset,const auto& value){emit(name,offset,value);};
        live("position",0x1bac,v.position);live("previous_position",0x1bbc,v.previous_position);
        live("orientation",0x1bcc,v.orientation);live("incremental",0x1c0c,v.incremental);live("secondary_rotation",0x1c4c,v.secondary_rotation);
        live("basis_x",0x1cbc,v.basis_x);live("basis_z",0x1cdc,v.basis_z);live("basis_y_w",0x1cd8,v.basis_y_w);
        live("velocity",0x1c8c,v.velocity);live("velocity_w",0x1c98,v.velocity_w);live("previous_velocity",0x1c9c,v.previous_velocity);live("previous_velocity_w",0x1ca8,v.previous_velocity_w);
        live("acceleration",0x1cac,v.acceleration);live("up",0x1ccc,v.up);
        live("total_speed",0x1cec,v.total_speed);live("speed",0x1cf0,v.speed);live("steering",0x1cf4,v.steering);live("throttle",0x1cf8,v.throttle);live("rpm",0x1cfc,v.rpm);
        live("engine_output",0x1d00,v.engine_output);live("engine_output_scale",0x1d04,v.engine_output_scale);live("engine_output_curve",0x1d08,v.engine_output_curve);
        live("torque",0x1f08,v.torque);live("torque_curve",0x1f0c,v.torque_curve);live("gear",0x1f10,v.gear);live("clutch",0x1f14,v.clutch);live("downshift_ticks",0x1f1c,v.downshift_ticks);
        live("drag_force",0x1f20,v.drag_force);live("lift_force",0x1f24,v.lift_force);live("drive_force",0x1f28,v.drive_force);live("air_factor",0x1fd0,v.air_factor);
        live("previous_contact_class",0x1fd4,v.previous_contact_class);live("contact_class",0x1fd8,v.contact_class);live("contact_hold",0x1fdc,v.contact_hold);
        for(unsigned i=0;i<4;++i){
            const auto at=0x1fe0+i*0x98;const auto name="wheel/"+std::to_string(i)+"/";const auto& w=v.wheels[i];
            emit(name+"acceleration",at,w.acceleration);emit(name+"speed",at+4,w.speed);emit(name+"slip",at+8,w.slip);emit(name+"load",at+12,w.load);emit(name+"reference_load",at+16,w.reference_load);
            emit(name+"lateral_speed",at+20,w.lateral_speed);emit(name+"rolling_force",at+28,w.rolling_force);emit(name+"grip_usage",at+48,w.grip_usage);
            emit(name+"static_friction",at+52,w.static_friction);emit(name+"kinetic_friction",at+56,w.kinetic_friction);emit(name+"contact_countdown",at+112,w.contact_countdown);
            emit(name+"surface_grip",at+132,w.surface_grip);emit(name+"surface_roughness",at+136,w.surface_roughness);emit(name+"surface_rolling",at+140,w.surface_rolling);
            emit(name+"contact_normal",at+76,w.contact_normal);emit(name+"braking_signal",0x2cf4+i*4,w.braking_signal);
        }
        live("turbo_active",0x2250,v.turbo_active);live("turbo_rpm",0x2254,v.turbo_rpm);live("turbo_force",0x2258,v.turbo_force);live("fuel",0x2270,v.fuel);
        live("control_flags",0x22b4,v.control_flags);live("input",0x22aa,v.input);live("previous_shift",0x22a4,v.previous_shift);live("previous_stunt",0x22a5,v.previous_stunt);
        live("contact_counter",0x71ec,v.contact_counter);live("airborne_ticks",0x71fc,v.airborne_ticks);live("brake_ramp",0x229c,v.brake_ramp);live("steering_ramp",0x2298,v.steering_ramp);
        live("brake_signal",0x28a8,v.brake_signal);live("steering_boost",0x22c4,v.steering_boost);
        constexpr std::array<unsigned,8> counters{0x33c4,0x3504,0x391c,0x3920,0x2f18,0x3130,0x2ad4,0x2cf0};
        for(unsigned i=0;i<counters.size();++i)emit("camera_effects/"+std::to_string(i),counters[i],v.camera_effects.values[i]);
        live("mass_override",0x762c,v.mass_override);live("performance_multiplier",0x2634,v.performance_multiplier);
        live("auxiliary_displacement",0x2308,v.auxiliary_displacement);live("previous_auxiliary_displacement",0x2318,v.previous_auxiliary_displacement);live("auxiliary_velocity",0x2328,v.auxiliary_velocity);
        const auto& t=v.stunt;
        live("stunt/phase_indices",0x6da4,t.phase_indices);live("stunt/history_and_balance",0x6db4,t.history_and_balance);
        live("stunt/flip_phase",0x71c4,t.flip_phase);live("stunt/flip_timer",0x71c8,t.flip_timer);live("stunt/flip_up",0x71cc,t.flip_up);
        live("stunt/drift_ticks",0x71dc,t.drift_ticks);live("stunt/slipstream_ticks",0x71e0,t.slipstream_ticks);live("stunt/roof_ticks",0x71e4,t.roof_ticks);
        live("stunt/inversion_timer",0x71e8,t.inversion_timer);live("stunt/inversion_latch",0x7700,t.inversion_latch);
        live("stunt/active",0x71f0,t.active);live("stunt/previous_active",0x71f4,t.previous_active);live("stunt/previous_contact_counter",0x71f8,t.previous_contact_counter);
        live("stunt/accumulated_angles",0x7204,t.accumulated_angles);live("stunt/angular_velocity",0x7214,t.angular_velocity);live("stunt/angular_acceleration",0x7224,t.angular_acceleration);
        live("stunt/launch_tick",0x7234,t.launch_tick);live("stunt/launch_height",0x7238,t.launch_height);live("stunt/maximum_height",0x723c,t.maximum_height);live("stunt/height_level",0x7240,t.height_level);
        live("stunt/launch_state",0x7244,t.launch_state);live("stunt/landing_pending",0x7248,t.landing_pending);live("stunt/landing_counter",0x724c,t.landing_counter);live("stunt/landing_hold",0x7250,t.landing_hold);live("stunt/score_hold",0x7254,t.score_hold);
        live("stunt/chain_count",0x7258,t.chain_count);live("stunt/displayed_chain_count",0x725c,t.displayed_chain_count);live("stunt/chain_decay_active",0x7260,t.chain_decay_active);live("stunt/chain_decay_timer",0x7264,t.chain_decay_timer);
        live("score",0x260c,v.score);live("air_score_rate",0x2610,v.air_score_rate);live("launch_score",0x2614,v.launch_score);live("stunt_statistics",0x2584,v.stunt_statistics);
        live("launch_effect_level",0x2260,v.launch_effect_level);live("launch_effect_strength",0x2264,v.launch_effect_strength);live("launch_effect_timer",0x2268,v.launch_effect_timer);
        live("launch_event",0x2394,v.launch_event);live("launch_event_strength",0x2398,v.launch_event_strength);live("stunt_meter",0x2678,v.stunt_meter);
        live("gear_changed",0x23a0,v.gear_changed);live("energy_exempt",0x24e0,v.energy_exempt);live("upgrade_active",0x2640,v.upgrade_active);live("upgrade_condition",0x2660,v.upgrade_condition);
        live("energy",0x2570,v.energy);live("energy_capacity",0x2574,v.energy_capacity);live("effective_mass",0x2618,v.effective_mass);live("effective_com_height",0x261c,v.effective_com_height);
        live("suspension",0x2240,v.suspension);live("body_heave",0x288c,v.body_heave);live("body_pitch",0x2890,v.body_pitch);live("body_roll",0x2894,v.body_roll);live("average_load",0x2898,v.average_load);live("average_grip",0x289c,v.average_grip);
        live("front_axle_position",0x284c,v.front_axle_position);live("rear_axle_position",0x285c,v.rear_axle_position);live("front_axle_velocity",0x286c,v.front_axle_velocity);live("rear_axle_velocity",0x287c,v.rear_axle_velocity);
        live("special_mode",0x7648,v.special_mode);
    }
}
}
