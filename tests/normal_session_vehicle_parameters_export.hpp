#pragma once
// Output-only six actual VehicleParameters owners. Source witnesses:
// reports/level-work/series6/normal-session-vehicle-parameters-codec-v1.
// No values are decoded, initialized, clamped or imported into Session.
#include "normal_session_race_export.hpp"
namespace normal_session_owner {
inline void export_vehicle_parameters_owners(RaceSession& session,Image& out,NativeAddresses& addresses){
    static_assert(std::endian::native==std::endian::little);
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
    static_assert(std::is_standard_layout_v<VehicleParameters> && std::is_trivially_copyable_v<VehicleParameters>);
    static_assert(sizeof(VehicleParameters)==7032 && sizeof(TyreParameters)==40 && sizeof(Curve)==512);
    const auto& bank=session.state().vehicles.parameters;
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(bank)>,std::array<VehicleParameters,6>>);
    // Each named member remains an explicit owner. Reject a full image
    // before adding anything if these 582 owners exceed the wire limit.
    out.require_allocation_capacity(582);
    for(unsigned slot=0;slot<bank.size();++slot){
        const auto base=race_export_detail::vehicle_address(slot);
        const auto key="vehicle-parameters/vehicle/"+std::to_string(slot)+"/";const auto& p=bank[slot];
        const auto live=[&](const std::string& member,std::uint32_t offset,const auto& value){
            const auto name=key+member;
            race_export_detail::field(out,addresses,name,add(base,offset),value);
            out.alias(name+"/actualNative",addresses.address(&value,sizeof(value)),narrow(sizeof(value)));
        };
        live("friction_curve",0x20,p.friction_curve);
        live("steering_curve",0x258,p.steering_curve);
        live("mass",0x220,p.mass);
        live("com_a",0x224,p.com_a);
        live("com_b",0x228,p.com_b);
        live("com_height",0x22c,p.com_height);
        live("wheelbase",0x230,p.wheelbase);
        live("track_front",0x234,p.track_front);
        live("track_rear",0x238,p.track_rear);
        live("length",0x23c,p.length);
        live("width",0x240,p.width);
        live("height",0x244,p.height);
        live("ride_height",0x248,p.ride_height);
        live("brake_bias",0xd5c,p.brake_bias);
        live("brake_acceleration",0xd58,p.brake_acceleration);
        live("steering_max",0x24c,p.steering_max);
        live("steering_acceleration",0x250,p.steering_acceleration);
        live("steering_sensitivity",0x254,p.steering_sensitivity);
        live("rpm_max",0x45c,p.rpm_max);
        live("rpm_idle",0x460,p.rpm_idle);
        live("rpm_limit",0x464,p.rpm_limit);
        live("engine_decay",0x468,p.engine_decay);
        live("max_torque",0x46c,p.max_torque);
        live("torque_curves",0x470,p.torque_curves);
        live("drive_mode",0xc70,p.drive_mode);
        live("gear_count",0xc74,p.gear_count);
        live("manual_gears",0xc78,p.manual_gears);
        live("final_drive",0xc7c,p.final_drive);
        live("shift_time",0xca0,p.shift_time);
        live("efficiency",0xca4,p.efficiency);
        live("power_split",0xca8,p.power_split);
        live("ratios",0xc80,p.ratios);
        live("drag_coefficient",0xcac,p.drag_coefficient);
        live("lift_coefficient",0xcb0,p.lift_coefficient);
        live("frontal_area",0xcb4,p.frontal_area);
        live("turbo_acceleration",0xd60,p.turbo_acceleration);
        live("turbo_max",0xd64,p.turbo_max);
        live("turbo_decay",0xd68,p.turbo_decay);
        live("turbo_scale",0xd6c,p.turbo_scale);
        live("turbo_curve",0xd70,p.turbo_curve);
        live("turbo_energy_threshold",0xf74,p.turbo_energy_threshold);
        live("turbo_consumption",0xf70,p.turbo_consumption);
        live("fuel_capacity",0xf78,p.fuel_capacity);
        live("fuel_unit_mass",0xf7c,p.fuel_unit_mass);
        live("fuel_consumption",0xf80,p.fuel_consumption);
        live("consumption_curve",0xf84,p.consumption_curve);
        live("refuel_rate",0x1184,p.refuel_rate);
        live("digital_steering",0x1188,p.digital_steering);
        live("digital_brake",0x1388,p.digital_brake);
        live("analog_steering",0x1588,p.analog_steering);
        live("analog_brake",0x1788,p.analog_brake);
        live("velocity_steering",0x1988,p.velocity_steering);
        live("launch_time",0x1b94,p.launch_time);
        live("launch_tolerance",0x1b98,p.launch_tolerance);
        live("launch_speed",0x1b9c,p.launch_speed);
        live("max_pitch_speed",0x1ba0,p.max_pitch_speed);
        live("max_yaw_speed",0x1ba4,p.max_yaw_speed);
        for(unsigned tyre=0;tyre<p.tyres.size();++tyre){
            const auto& t=p.tyres[tyre];const auto tyre_key="tyre/"+std::to_string(tyre)+"/";
            live(tyre_key+"radius",0xcb8+tyre*16,t.radius);
            live(tyre_key+"pressure",0xcbc+tyre*16,t.pressure);
            live(tyre_key+"static_friction",0xcc0+tyre*16,t.static_friction);
            live(tyre_key+"kinetic_friction",0xcc4+tyre*16,t.kinetic_friction);
            live(tyre_key+"travel_in",0xcf8+tyre*24,t.travel_in);
            live(tyre_key+"damping_in",0xcfc+tyre*24,t.damping_in);
            live(tyre_key+"stiffness_in",0xd00+tyre*24,t.stiffness_in);
            live(tyre_key+"travel_out",0xd04+tyre*24,t.travel_out);
            live(tyre_key+"damping_out",0xd08+tyre*24,t.damping_out);
            live(tyre_key+"stiffness_out",0xd0c+tyre*24,t.stiffness_out);
        }
    }
}
}
