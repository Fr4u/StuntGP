#pragma once
#include "sgn/vehicle.hpp"
#include <bit>
#include <cstring>

// Test-only bridge. The caller supplies at least 0x7800 bytes.
// Maps every native VehicleState/VehicleParameters field; unmodeled words remain intact.
namespace sgn::retail::oracle {
template<class Record>
void transfer(Record& b,VehicleParameters& p,VehicleState& s,bool read) {
    if(b.size()<0x7800)throw std::runtime_error("vehicle bridge requires a complete record");
    auto word=[](const Record& data,std::size_t at){std::uint32_t value;std::memcpy(&value,data.data()+at,4);return value;};
    auto put=[](Record& data,std::size_t at,std::uint32_t value){std::memcpy(data.data()+at,&value,4);};
    auto real=[&](const Record& data,std::size_t at){return std::bit_cast<float>(word(data,at));};
    auto put_real=[&](Record& data,std::size_t at,float value){put(data,at,std::bit_cast<std::uint32_t>(value));};
    auto f=[&](float& x,std::size_t at){if(read)x=real(b,at);else put_real(b,at,x);};
    auto i=[&](std::int32_t& x,std::size_t at){if(read)x=std::bit_cast<std::int32_t>(word(b,at));else put(b,at,std::bit_cast<std::uint32_t>(x));};
    auto u=[&](std::uint32_t& x,std::size_t at){if(read)x=word(b,at);else put(b,at,x);};
    auto curve=[&](Curve& x,std::size_t at){for(unsigned j=0;j<128;++j) f(x[j],at+j*4);};
    auto v=[&](VehicleVector& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);f(x.w,at+12);};
    auto xyz=[&](sgn::Vec3& x,std::size_t at){f(x.x,at);f(x.y,at+4);f(x.z,at+8);};
    curve(p.friction_curve,0x20); curve(p.steering_curve,0x258);
    f(p.mass,0x220);f(p.com_a,0x224);f(p.com_b,0x228);f(p.com_height,0x22c);
    f(p.wheelbase,0x230);f(p.track_front,0x234);f(p.track_rear,0x238);
    f(p.length,0x23c);f(p.width,0x240);f(p.height,0x244);f(p.ride_height,0x248);
    f(p.brake_bias,0xd5c);f(p.brake_acceleration,0xd58);
    f(p.steering_max,0x24c);f(p.steering_acceleration,0x250);f(p.steering_sensitivity,0x254);
    f(p.rpm_max,0x45c);f(p.rpm_idle,0x460);f(p.rpm_limit,0x464);f(p.engine_decay,0x468);f(p.max_torque,0x46c);
    for(unsigned j=0;j<4;++j) curve(p.torque_curves[j],0x470+j*512);
    i(p.drive_mode,0xc70);i(p.gear_count,0xc74);i(p.manual_gears,0xc78);f(p.final_drive,0xc7c);
    for(unsigned j=0;j<8;++j) f(p.ratios[j],0xc80+j*4);
    f(p.shift_time,0xca0);f(p.efficiency,0xca4);f(p.power_split,0xca8);
    f(p.drag_coefficient,0xcac);f(p.lift_coefficient,0xcb0);f(p.frontal_area,0xcb4);
    for(unsigned j=0;j<4;++j) {auto& t=p.tyres[j];auto at=0xcb8+j*16;f(t.radius,at);f(t.pressure,at+4);f(t.static_friction,at+8);f(t.kinetic_friction,at+12);
        at=0xcf8+j*24;f(t.travel_in,at);f(t.damping_in,at+4);f(t.stiffness_in,at+8);f(t.travel_out,at+12);f(t.damping_out,at+16);f(t.stiffness_out,at+20);
    }
    f(p.turbo_acceleration,0xd60);f(p.turbo_max,0xd64);f(p.turbo_decay,0xd68);f(p.turbo_scale,0xd6c);
    curve(p.turbo_curve,0xd70);f(p.turbo_energy_threshold,0xf74);
    f(p.turbo_consumption,0xf70);f(p.fuel_capacity,0xf78);f(p.fuel_unit_mass,0xf7c);f(p.fuel_consumption,0xf80);curve(p.consumption_curve,0xf84);
    f(p.refuel_rate,0x1184);curve(p.digital_steering,0x1188);curve(p.digital_brake,0x1388);curve(p.analog_steering,0x1588);curve(p.analog_brake,0x1788);curve(p.velocity_steering,0x1988);
    f(p.launch_time,0x1b94);f(p.launch_tolerance,0x1b98);f(p.launch_speed,0x1b9c);f(p.max_pitch_speed,0x1ba0);f(p.max_yaw_speed,0x1ba4);
    v(s.position,0x1bac);v(s.previous_position,0x1bbc);
    for(unsigned j=0;j<16;++j){f(s.orientation[j],0x1bcc+j*4);f(s.incremental[j],0x1c0c+j*4);f(s.secondary_rotation[j],0x1c4c+j*4);}
    v(s.basis_x,0x1cbc);v(s.basis_z,0x1cdc);f(s.basis_y_w,0x1cd8);
    xyz(s.velocity,0x1c8c);f(s.velocity_w,0x1c98);xyz(s.previous_velocity,0x1c9c);f(s.previous_velocity_w,0x1ca8);xyz(s.acceleration,0x1cac);
    f(s.up.x,0x1ccc);f(s.up.y,0x1cd0);f(s.up.z,0x1cd4);
    f(s.total_speed,0x1cec);f(s.speed,0x1cf0);f(s.steering,0x1cf4);f(s.throttle,0x1cf8);f(s.rpm,0x1cfc);
    f(s.engine_output,0x1d00);f(s.engine_output_scale,0x1d04);curve(s.engine_output_curve,0x1d08);
    f(s.torque,0x1f08);i(s.torque_curve,0x1f0c);i(s.gear,0x1f10);f(s.clutch,0x1f14);i(s.downshift_ticks,0x1f1c);
    f(s.drag_force,0x1f20);f(s.lift_force,0x1f24);f(s.drive_force,0x1f28);f(s.air_factor,0x1fd0);
    i(s.previous_contact_class,0x1fd4);i(s.contact_class,0x1fd8);i(s.contact_hold,0x1fdc);
    for(unsigned j=0;j<4;++j) {auto& w=s.wheels[j];auto at=0x1fe0+j*0x98;
        f(w.acceleration,at);f(w.speed,at+4);f(w.slip,at+8);f(w.load,at+12);f(w.reference_load,at+16);
        f(w.lateral_speed,at+20);f(w.rolling_force,at+28);f(w.grip_usage,at+48);
        f(w.static_friction,at+52);f(w.kinetic_friction,at+56);i(w.contact_countdown,at+112);
        f(w.surface_grip,at+132);f(w.surface_roughness,at+136);f(w.surface_rolling,at+140);
        v(w.contact_normal,at+76);
        f(w.braking_signal,0x2cf4+j*4);
    }
    i(s.turbo_active,0x2250);f(s.turbo_rpm,0x2254);f(s.turbo_force,0x2258);
    f(s.fuel,0x2270);for(unsigned j=0;j<4;++j)i(s.control_flags[j],0x22b4+j*4);
    std::size_t input_at=0x22aa;
    for(auto* byte:{&s.input.throttle,&s.input.brake,&s.input.left,&s.input.right,&s.input.shift,&s.input.stunt,&s.input.rear_lock,&s.input.steering_boost,&s.input.turbo,&s.input.auxiliary}) {
        if(read)*byte=b[input_at];else b[input_at]=*byte;++input_at;
    }
    if(read){s.previous_shift=b[0x22a4];s.previous_stunt=b[0x22a5];}
    else {b[0x22a4]=s.previous_shift;b[0x22a5]=s.previous_stunt;}
    i(s.contact_counter,0x71ec);u(s.airborne_ticks,0x71fc);i(s.brake_ramp,0x229c);i(s.steering_ramp,0x2298);
    i(s.brake_signal,0x28a8);f(s.steering_boost,0x22c4);
    const std::array<std::size_t,8> effects={0x33c4,0x3504,0x391c,0x3920,0x2f18,0x3130,0x2ad4,0x2cf0};
    for(unsigned j=0;j<effects.size();++j)u(s.camera_effects.values[j],effects[j]);
    i(s.mass_override,0x762c);f(s.performance_multiplier,0x2634);
    v(s.auxiliary_displacement,0x2308);v(s.previous_auxiliary_displacement,0x2318);v(s.auxiliary_velocity,0x2328);
    for(unsigned j=0;j<4;++j)i(s.stunt.phase_indices[j],0x6da4+j*4);
    for(unsigned j=0;j<260;++j)u(s.stunt.history_and_balance[j],0x6db4+j*4);
    u(s.stunt.flip_phase,0x71c4);u(s.stunt.flip_timer,0x71c8);v(s.stunt.flip_up,0x71cc);
    u(s.stunt.drift_ticks,0x71dc);u(s.stunt.slipstream_ticks,0x71e0);u(s.stunt.roof_ticks,0x71e4);
    u(s.stunt.inversion_timer,0x71e8);u(s.stunt.inversion_latch,0x7700);
    i(s.stunt.active,0x71f0);i(s.stunt.previous_active,0x71f4);i(s.stunt.previous_contact_counter,0x71f8);
    v(s.stunt.accumulated_angles,0x7204);v(s.stunt.angular_velocity,0x7214);v(s.stunt.angular_acceleration,0x7224);
    u(s.stunt.launch_tick,0x7234);f(s.stunt.launch_height,0x7238);f(s.stunt.maximum_height,0x723c);u(s.stunt.height_level,0x7240);
    u(s.stunt.launch_state,0x7244);u(s.stunt.landing_pending,0x7248);u(s.stunt.landing_counter,0x724c);u(s.stunt.landing_hold,0x7250);u(s.stunt.score_hold,0x7254);
    u(s.stunt.chain_count,0x7258);u(s.stunt.displayed_chain_count,0x725c);u(s.stunt.chain_decay_active,0x7260);u(s.stunt.chain_decay_timer,0x7264);
    f(s.score,0x260c);f(s.air_score_rate,0x2610);f(s.launch_score,0x2614);
    for(unsigned j=0;j<4;++j){auto& stat=s.stunt_statistics[j];const auto at=0x2584+j*16;
        u(stat.count,at);f(stat.last,at+4);f(stat.total,at+8);f(stat.average,at+12);}
    i(s.launch_effect_level,0x2260);f(s.launch_effect_strength,0x2264);u(s.launch_effect_timer,0x2268);
    u(s.launch_event,0x2394);f(s.launch_event_strength,0x2398);u(s.stunt_meter,0x2678);
    i(s.gear_changed,0x23a0);i(s.energy_exempt,0x24e0);i(s.upgrade_active,0x2640);f(s.upgrade_condition,0x2660);
    f(s.energy,0x2570);f(s.energy_capacity,0x2574);f(s.effective_mass,0x2618);f(s.effective_com_height,0x261c);
    for(unsigned j=0;j<4;++j)f(s.suspension[j],0x2240+j*4);
    f(s.body_heave,0x288c);f(s.body_pitch,0x2890);f(s.body_roll,0x2894);f(s.average_load,0x2898);f(s.average_grip,0x289c);
    v(s.front_axle_position,0x284c);v(s.rear_axle_position,0x285c);v(s.front_axle_velocity,0x286c);v(s.rear_axle_velocity,0x287c);
    i(s.special_mode,0x7648);
}
}
