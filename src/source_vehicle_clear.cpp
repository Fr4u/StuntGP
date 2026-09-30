#include "sgn/source_vehicle_clear.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include <bit>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace sgn::retail {
namespace {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
static_assert(std::bit_cast<std::uint32_t>(0.0f)==0);
static_assert(VehicleStorage::physical_slots==6);

// Source scalar words/bytes, never native object representations. Every
// overload follows increasing source lane order and is non-allocating.
// A source NULL is an empty optional, not an engaged identity with index zero.
void zero(float& v) noexcept {v=0.0f;}
void zero(std::uint32_t& v) noexcept {v=0;}
void zero(std::int32_t& v) noexcept {v=0;}
void zero(std::uint8_t& v) noexcept {v=0;}
void zero(char& v) noexcept {v=0;}
void zero(VehicleVector& v) noexcept {zero(v.x);zero(v.y);zero(v.z);zero(v.w);}
void zero(CameraVector& v) noexcept {zero(v.x);zero(v.y);zero(v.z);zero(v.w);}
void zero(Vec3& v) noexcept {zero(v.x);zero(v.y);zero(v.z);}
void zero(contact::Vector4& v) noexcept {zero(v.x);zero(v.y);zero(v.z);zero(v.opaque);}
void zero(VehicleInput& v) noexcept {
    zero(v.throttle);zero(v.brake);zero(v.left);zero(v.right);
    zero(v.shift);zero(v.stunt);zero(v.rear_lock);zero(v.steering_boost);
    zero(v.turbo);zero(v.auxiliary);
}
void zero(VehicleSegment& v) noexcept {zero(v.angles);zero(v.velocity);zero(v.direction);}
void zero(VehicleStatistic& v) noexcept {zero(v.count);zero(v.last);zero(v.total);zero(v.average);}
template<class T> void zero(std::optional<T>& v) noexcept {
    static_assert(std::is_trivially_destructible_v<T>);
    v.reset();
}
template<class T,std::size_t N> void zero(std::array<T,N>& v) noexcept {
    for(auto& lane:v)zero(lane);
}

// Transcribed field map: monotonically increasing offsets in each physical
// record, then the next record (49A1F8 + slot*7724). No owning world copy,
// allocation, validation, I/O, callback or resource destruction inside.
// Split host owners cannot promise the x86 DWORD's hardware atomicity. Source
// REP has no observer in the middle; the borrowed platform is thread-confined.
void clear_source_records(RaceSessionState& state) noexcept {
    auto& v=state.vehicles;
    for(unsigned slot=0;slot!=6;++slot) {
        // 0000..001F: configuration_identity.header
        zero(v.configuration_identity[slot].header);
        // 0020..021F: parameters.friction_curve
        zero(v.parameters[slot].friction_curve);
        // 0220..0223: parameters.mass
        zero(v.parameters[slot].mass);
        // 0224..0227: parameters.com_a
        zero(v.parameters[slot].com_a);
        // 0228..022B: parameters.com_b
        zero(v.parameters[slot].com_b);
        // 022C..022F: parameters.com_height
        zero(v.parameters[slot].com_height);
        // 0230..0233: parameters.wheelbase
        zero(v.parameters[slot].wheelbase);
        // 0234..0237: parameters.track_front
        zero(v.parameters[slot].track_front);
        // 0238..023B: parameters.track_rear
        zero(v.parameters[slot].track_rear);
        // 023C..023F: parameters.length
        zero(v.parameters[slot].length);
        // 0240..0243: parameters.width
        zero(v.parameters[slot].width);
        // 0244..0247: parameters.height
        zero(v.parameters[slot].height);
        // 0248..024B: parameters.ride_height
        zero(v.parameters[slot].ride_height);
        // 024C..024F: parameters.steering_max
        zero(v.parameters[slot].steering_max);
        // 0250..0253: parameters.steering_acceleration
        zero(v.parameters[slot].steering_acceleration);
        // 0254..0257: parameters.steering_sensitivity
        zero(v.parameters[slot].steering_sensitivity);
        // 0258..0457: parameters.steering_curve
        zero(v.parameters[slot].steering_curve);
        // 0458..045B: source_uninterpreted.offset_0458
        zero(v.source_uninterpreted[slot].offset_0458);
        // 045C..045F: parameters.rpm_max
        zero(v.parameters[slot].rpm_max);
        // 0460..0463: parameters.rpm_idle
        zero(v.parameters[slot].rpm_idle);
        // 0464..0467: parameters.rpm_limit
        zero(v.parameters[slot].rpm_limit);
        // 0468..046B: parameters.engine_decay
        zero(v.parameters[slot].engine_decay);
        // 046C..046F: parameters.max_torque
        zero(v.parameters[slot].max_torque);
        // 0470..0C6F: parameters.torque_curves
        zero(v.parameters[slot].torque_curves);
        // 0C70..0C73: parameters.drive_mode
        zero(v.parameters[slot].drive_mode);
        // 0C74..0C77: parameters.gear_count
        zero(v.parameters[slot].gear_count);
        // 0C78..0C7B: parameters.manual_gears
        zero(v.parameters[slot].manual_gears);
        // 0C7C..0C7F: parameters.final_drive
        zero(v.parameters[slot].final_drive);
        // 0C80..0C9F: parameters.ratios
        zero(v.parameters[slot].ratios);
        // 0CA0..0CA3: parameters.shift_time
        zero(v.parameters[slot].shift_time);
        // 0CA4..0CA7: parameters.efficiency
        zero(v.parameters[slot].efficiency);
        // 0CA8..0CAB: parameters.power_split
        zero(v.parameters[slot].power_split);
        // 0CAC..0CAF: parameters.drag_coefficient
        zero(v.parameters[slot].drag_coefficient);
        // 0CB0..0CB3: parameters.lift_coefficient
        zero(v.parameters[slot].lift_coefficient);
        // 0CB4..0CB7: parameters.frontal_area
        zero(v.parameters[slot].frontal_area);
        // 0CB8..0CBB: parameters.tyres[0].radius
        zero(v.parameters[slot].tyres[0].radius);
        // 0CBC..0CBF: parameters.tyres[0].pressure
        zero(v.parameters[slot].tyres[0].pressure);
        // 0CC0..0CC3: parameters.tyres[0].static_friction
        zero(v.parameters[slot].tyres[0].static_friction);
        // 0CC4..0CC7: parameters.tyres[0].kinetic_friction
        zero(v.parameters[slot].tyres[0].kinetic_friction);
        // 0CC8..0CCB: parameters.tyres[1].radius
        zero(v.parameters[slot].tyres[1].radius);
        // 0CCC..0CCF: parameters.tyres[1].pressure
        zero(v.parameters[slot].tyres[1].pressure);
        // 0CD0..0CD3: parameters.tyres[1].static_friction
        zero(v.parameters[slot].tyres[1].static_friction);
        // 0CD4..0CD7: parameters.tyres[1].kinetic_friction
        zero(v.parameters[slot].tyres[1].kinetic_friction);
        // 0CD8..0CDB: parameters.tyres[2].radius
        zero(v.parameters[slot].tyres[2].radius);
        // 0CDC..0CDF: parameters.tyres[2].pressure
        zero(v.parameters[slot].tyres[2].pressure);
        // 0CE0..0CE3: parameters.tyres[2].static_friction
        zero(v.parameters[slot].tyres[2].static_friction);
        // 0CE4..0CE7: parameters.tyres[2].kinetic_friction
        zero(v.parameters[slot].tyres[2].kinetic_friction);
        // 0CE8..0CEB: parameters.tyres[3].radius
        zero(v.parameters[slot].tyres[3].radius);
        // 0CEC..0CEF: parameters.tyres[3].pressure
        zero(v.parameters[slot].tyres[3].pressure);
        // 0CF0..0CF3: parameters.tyres[3].static_friction
        zero(v.parameters[slot].tyres[3].static_friction);
        // 0CF4..0CF7: parameters.tyres[3].kinetic_friction
        zero(v.parameters[slot].tyres[3].kinetic_friction);
        // 0CF8..0CFB: parameters.tyres[0].travel_in
        zero(v.parameters[slot].tyres[0].travel_in);
        // 0CFC..0CFF: parameters.tyres[0].damping_in
        zero(v.parameters[slot].tyres[0].damping_in);
        // 0D00..0D03: parameters.tyres[0].stiffness_in
        zero(v.parameters[slot].tyres[0].stiffness_in);
        // 0D04..0D07: parameters.tyres[0].travel_out
        zero(v.parameters[slot].tyres[0].travel_out);
        // 0D08..0D0B: parameters.tyres[0].damping_out
        zero(v.parameters[slot].tyres[0].damping_out);
        // 0D0C..0D0F: parameters.tyres[0].stiffness_out
        zero(v.parameters[slot].tyres[0].stiffness_out);
        // 0D10..0D13: parameters.tyres[1].travel_in
        zero(v.parameters[slot].tyres[1].travel_in);
        // 0D14..0D17: parameters.tyres[1].damping_in
        zero(v.parameters[slot].tyres[1].damping_in);
        // 0D18..0D1B: parameters.tyres[1].stiffness_in
        zero(v.parameters[slot].tyres[1].stiffness_in);
        // 0D1C..0D1F: parameters.tyres[1].travel_out
        zero(v.parameters[slot].tyres[1].travel_out);
        // 0D20..0D23: parameters.tyres[1].damping_out
        zero(v.parameters[slot].tyres[1].damping_out);
        // 0D24..0D27: parameters.tyres[1].stiffness_out
        zero(v.parameters[slot].tyres[1].stiffness_out);
        // 0D28..0D2B: parameters.tyres[2].travel_in
        zero(v.parameters[slot].tyres[2].travel_in);
        // 0D2C..0D2F: parameters.tyres[2].damping_in
        zero(v.parameters[slot].tyres[2].damping_in);
        // 0D30..0D33: parameters.tyres[2].stiffness_in
        zero(v.parameters[slot].tyres[2].stiffness_in);
        // 0D34..0D37: parameters.tyres[2].travel_out
        zero(v.parameters[slot].tyres[2].travel_out);
        // 0D38..0D3B: parameters.tyres[2].damping_out
        zero(v.parameters[slot].tyres[2].damping_out);
        // 0D3C..0D3F: parameters.tyres[2].stiffness_out
        zero(v.parameters[slot].tyres[2].stiffness_out);
        // 0D40..0D43: parameters.tyres[3].travel_in
        zero(v.parameters[slot].tyres[3].travel_in);
        // 0D44..0D47: parameters.tyres[3].damping_in
        zero(v.parameters[slot].tyres[3].damping_in);
        // 0D48..0D4B: parameters.tyres[3].stiffness_in
        zero(v.parameters[slot].tyres[3].stiffness_in);
        // 0D4C..0D4F: parameters.tyres[3].travel_out
        zero(v.parameters[slot].tyres[3].travel_out);
        // 0D50..0D53: parameters.tyres[3].damping_out
        zero(v.parameters[slot].tyres[3].damping_out);
        // 0D54..0D57: parameters.tyres[3].stiffness_out
        zero(v.parameters[slot].tyres[3].stiffness_out);
        // 0D58..0D5B: parameters.brake_acceleration
        zero(v.parameters[slot].brake_acceleration);
        // 0D5C..0D5F: parameters.brake_bias
        zero(v.parameters[slot].brake_bias);
        // 0D60..0D63: parameters.turbo_acceleration
        zero(v.parameters[slot].turbo_acceleration);
        // 0D64..0D67: parameters.turbo_max
        zero(v.parameters[slot].turbo_max);
        // 0D68..0D6B: parameters.turbo_decay
        zero(v.parameters[slot].turbo_decay);
        // 0D6C..0D6F: parameters.turbo_scale
        zero(v.parameters[slot].turbo_scale);
        // 0D70..0F6F: parameters.turbo_curve
        zero(v.parameters[slot].turbo_curve);
        // 0F70..0F73: parameters.turbo_consumption
        zero(v.parameters[slot].turbo_consumption);
        // 0F74..0F77: parameters.turbo_energy_threshold
        zero(v.parameters[slot].turbo_energy_threshold);
        // 0F78..0F7B: parameters.fuel_capacity
        zero(v.parameters[slot].fuel_capacity);
        // 0F7C..0F7F: parameters.fuel_unit_mass
        zero(v.parameters[slot].fuel_unit_mass);
        // 0F80..0F83: parameters.fuel_consumption
        zero(v.parameters[slot].fuel_consumption);
        // 0F84..1183: parameters.consumption_curve
        zero(v.parameters[slot].consumption_curve);
        // 1184..1187: parameters.refuel_rate
        zero(v.parameters[slot].refuel_rate);
        // 1188..1387: parameters.digital_steering
        zero(v.parameters[slot].digital_steering);
        // 1388..1587: parameters.digital_brake
        zero(v.parameters[slot].digital_brake);
        // 1588..1787: parameters.analog_steering
        zero(v.parameters[slot].analog_steering);
        // 1788..1987: parameters.analog_brake
        zero(v.parameters[slot].analog_brake);
        // 1988..1B87: parameters.velocity_steering
        zero(v.parameters[slot].velocity_steering);
        // 1B88..1B8B: source_uninterpreted.offset_1b88
        zero(v.source_uninterpreted[slot].offset_1b88);
        // 1B8C..1B8F: drivers.skill
        zero(v.drivers[slot].skill);
        // 1B90..1B93: drivers.mode
        zero(v.drivers[slot].mode);
        // 1B94..1B97: parameters.launch_time
        zero(v.parameters[slot].launch_time);
        // 1B98..1B9B: parameters.launch_tolerance
        zero(v.parameters[slot].launch_tolerance);
        // 1B9C..1B9F: parameters.launch_speed
        zero(v.parameters[slot].launch_speed);
        // 1BA0..1BA3: parameters.max_pitch_speed
        zero(v.parameters[slot].max_pitch_speed);
        // 1BA4..1BA7: parameters.max_yaw_speed
        zero(v.parameters[slot].max_yaw_speed);
        // 1BA8..1BAB: race.active
        zero(v.race[slot].active);
        // 1BAC..1BBB: physics.position
        zero(v.physics[slot].position);
        // 1BBC..1BCB: physics.previous_position
        zero(v.physics[slot].previous_position);
        // 1BCC..1C0B: physics.orientation
        zero(v.physics[slot].orientation);
        // 1C0C..1C4B: physics.incremental
        zero(v.physics[slot].incremental);
        // 1C4C..1C8B: physics.secondary_rotation
        zero(v.physics[slot].secondary_rotation);
        // 1C8C..1C97: physics.velocity
        zero(v.physics[slot].velocity);
        // 1C98..1C9B: physics.velocity_w
        zero(v.physics[slot].velocity_w);
        // 1C9C..1CA7: physics.previous_velocity
        zero(v.physics[slot].previous_velocity);
        // 1CA8..1CAB: physics.previous_velocity_w
        zero(v.physics[slot].previous_velocity_w);
        // 1CAC..1CB7: physics.acceleration
        zero(v.physics[slot].acceleration);
        // 1CB8..1CBB: source_uninterpreted.offset_1cb8
        zero(v.source_uninterpreted[slot].offset_1cb8);
        // 1CBC..1CCB: physics.basis_x
        zero(v.physics[slot].basis_x);
        // 1CCC..1CD7: physics.up
        zero(v.physics[slot].up);
        // 1CD8..1CDB: physics.basis_y_w
        zero(v.physics[slot].basis_y_w);
        // 1CDC..1CEB: physics.basis_z
        zero(v.physics[slot].basis_z);
        // 1CEC..1CEF: physics.total_speed
        zero(v.physics[slot].total_speed);
        // 1CF0..1CF3: physics.speed
        zero(v.physics[slot].speed);
        // 1CF4..1CF7: physics.steering
        zero(v.physics[slot].steering);
        // 1CF8..1CFB: physics.throttle
        zero(v.physics[slot].throttle);
        // 1CFC..1CFF: physics.rpm
        zero(v.physics[slot].rpm);
        // 1D00..1D03: physics.engine_output
        zero(v.physics[slot].engine_output);
        // 1D04..1D07: physics.engine_output_scale
        zero(v.physics[slot].engine_output_scale);
        // 1D08..1F07: physics.engine_output_curve
        zero(v.physics[slot].engine_output_curve);
        // 1F08..1F0B: physics.torque
        zero(v.physics[slot].torque);
        // 1F0C..1F0F: physics.torque_curve
        zero(v.physics[slot].torque_curve);
        // 1F10..1F13: physics.gear
        zero(v.physics[slot].gear);
        // 1F14..1F17: physics.clutch
        zero(v.physics[slot].clutch);
        // 1F18..1F1B: source_uninterpreted.offset_1f18
        zero(v.source_uninterpreted[slot].offset_1f18);
        // 1F1C..1F1F: physics.downshift_ticks
        zero(v.physics[slot].downshift_ticks);
        // 1F20..1F23: physics.drag_force
        zero(v.physics[slot].drag_force);
        // 1F24..1F27: physics.lift_force
        zero(v.physics[slot].lift_force);
        // 1F28..1F2B: physics.drive_force
        zero(v.physics[slot].drive_force);
        // 1F2C..1F3B: history.samples[0].position
        zero(v.history[slot].samples[0].position);
        // 1F3C..1F3F: history.samples[0].radius
        zero(v.history[slot].samples[0].radius);
        // 1F40..1F4F: history.samples[1].position
        zero(v.history[slot].samples[1].position);
        // 1F50..1F53: history.samples[1].radius
        zero(v.history[slot].samples[1].radius);
        // 1F54..1F63: history.samples[2].position
        zero(v.history[slot].samples[2].position);
        // 1F64..1F67: history.samples[2].radius
        zero(v.history[slot].samples[2].radius);
        // 1F68..1F77: history.samples[3].position
        zero(v.history[slot].samples[3].position);
        // 1F78..1F7B: history.samples[3].radius
        zero(v.history[slot].samples[3].radius);
        // 1F7C..1F8B: history.samples[4].position
        zero(v.history[slot].samples[4].position);
        // 1F8C..1F8F: history.samples[4].radius
        zero(v.history[slot].samples[4].radius);
        // 1F90..1F9F: history.samples[5].position
        zero(v.history[slot].samples[5].position);
        // 1FA0..1FA3: history.samples[5].radius
        zero(v.history[slot].samples[5].radius);
        // 1FA4..1FB3: history.samples[6].position
        zero(v.history[slot].samples[6].position);
        // 1FB4..1FB7: history.samples[6].radius
        zero(v.history[slot].samples[6].radius);
        // 1FB8..1FC7: history.samples[7].position
        zero(v.history[slot].samples[7].position);
        // 1FC8..1FCB: history.samples[7].radius
        zero(v.history[slot].samples[7].radius);
        // 1FCC..1FCF: history.ticks
        zero(v.history[slot].ticks);
        // 1FD0..1FD3: physics.air_factor
        zero(v.physics[slot].air_factor);
        // 1FD4..1FD7: physics.previous_contact_class
        zero(v.physics[slot].previous_contact_class);
        // 1FD8..1FDB: physics.contact_class
        zero(v.physics[slot].contact_class);
        // 1FDC..1FDF: physics.contact_hold
        zero(v.physics[slot].contact_hold);
        // 1FE0..1FE3: physics.wheels[0].acceleration
        zero(v.physics[slot].wheels[0].acceleration);
        // 1FE4..1FE7: physics.wheels[0].speed
        zero(v.physics[slot].wheels[0].speed);
        // 1FE8..1FEB: physics.wheels[0].slip
        zero(v.physics[slot].wheels[0].slip);
        // 1FEC..1FEF: physics.wheels[0].load
        zero(v.physics[slot].wheels[0].load);
        // 1FF0..1FF3: physics.wheels[0].reference_load
        zero(v.physics[slot].wheels[0].reference_load);
        // 1FF4..1FF7: physics.wheels[0].lateral_speed
        zero(v.physics[slot].wheels[0].lateral_speed);
        // 1FF8..1FFB: particles.field_1ff8[0]
        zero(v.particles[slot].field_1ff8[0]);
        // 1FFC..1FFF: physics.wheels[0].rolling_force
        zero(v.physics[slot].wheels[0].rolling_force);
        // 2000..200F: source_uninterpreted.offset_2000
        zero(v.source_uninterpreted[slot].offset_2000);
        // 2010..2013: physics.wheels[0].grip_usage
        zero(v.physics[slot].wheels[0].grip_usage);
        // 2014..2017: physics.wheels[0].static_friction
        zero(v.physics[slot].wheels[0].static_friction);
        // 2018..201B: physics.wheels[0].kinetic_friction
        zero(v.physics[slot].wheels[0].kinetic_friction);
        // 201C..202B: presentation.wheel_centers[0]
        zero(v.presentation[slot].wheel_centers[0]);
        // 202C..203B: physics.wheels[0].contact_normal
        zero(v.physics[slot].wheels[0].contact_normal);
        // 203C..204F: source_uninterpreted.offset_203c
        zero(v.source_uninterpreted[slot].offset_203c);
        // 2050..2053: physics.wheels[0].contact_countdown
        zero(v.physics[slot].wheels[0].contact_countdown);
        // 2054..2057: contact.wheels[0].previous_type
        zero(v.contact[slot].wheels[0].previous_type);
        // 2058..205B: contact.wheels[0].type
        zero(v.contact[slot].wheels[0].type);
        // 205C..205F: contact.wheels[0].changed
        zero(v.contact[slot].wheels[0].changed);
        // 2060..2063: contact.wheels[0].current_load
        zero(v.contact[slot].wheels[0].current_load);
        // 2064..2067: physics.wheels[0].surface_grip
        zero(v.physics[slot].wheels[0].surface_grip);
        // 2068..206B: physics.wheels[0].surface_roughness
        zero(v.physics[slot].wheels[0].surface_roughness);
        // 206C..206F: physics.wheels[0].surface_rolling
        zero(v.physics[slot].wheels[0].surface_rolling);
        // 2070..2073: wheels[0].previous_angle
        zero(v.wheels[slot][0].previous_angle);
        // 2074..2077: wheels[0].current_angle
        zero(v.wheels[slot][0].current_angle);
        // 2078..207B: physics.wheels[1].acceleration
        zero(v.physics[slot].wheels[1].acceleration);
        // 207C..207F: physics.wheels[1].speed
        zero(v.physics[slot].wheels[1].speed);
        // 2080..2083: physics.wheels[1].slip
        zero(v.physics[slot].wheels[1].slip);
        // 2084..2087: physics.wheels[1].load
        zero(v.physics[slot].wheels[1].load);
        // 2088..208B: physics.wheels[1].reference_load
        zero(v.physics[slot].wheels[1].reference_load);
        // 208C..208F: physics.wheels[1].lateral_speed
        zero(v.physics[slot].wheels[1].lateral_speed);
        // 2090..2093: particles.field_1ff8[1]
        zero(v.particles[slot].field_1ff8[1]);
        // 2094..2097: physics.wheels[1].rolling_force
        zero(v.physics[slot].wheels[1].rolling_force);
        // 2098..20A7: source_uninterpreted.offset_2098
        zero(v.source_uninterpreted[slot].offset_2098);
        // 20A8..20AB: physics.wheels[1].grip_usage
        zero(v.physics[slot].wheels[1].grip_usage);
        // 20AC..20AF: physics.wheels[1].static_friction
        zero(v.physics[slot].wheels[1].static_friction);
        // 20B0..20B3: physics.wheels[1].kinetic_friction
        zero(v.physics[slot].wheels[1].kinetic_friction);
        // 20B4..20C3: presentation.wheel_centers[1]
        zero(v.presentation[slot].wheel_centers[1]);
        // 20C4..20D3: physics.wheels[1].contact_normal
        zero(v.physics[slot].wheels[1].contact_normal);
        // 20D4..20E7: source_uninterpreted.offset_20d4
        zero(v.source_uninterpreted[slot].offset_20d4);
        // 20E8..20EB: physics.wheels[1].contact_countdown
        zero(v.physics[slot].wheels[1].contact_countdown);
        // 20EC..20EF: contact.wheels[1].previous_type
        zero(v.contact[slot].wheels[1].previous_type);
        // 20F0..20F3: contact.wheels[1].type
        zero(v.contact[slot].wheels[1].type);
        // 20F4..20F7: contact.wheels[1].changed
        zero(v.contact[slot].wheels[1].changed);
        // 20F8..20FB: contact.wheels[1].current_load
        zero(v.contact[slot].wheels[1].current_load);
        // 20FC..20FF: physics.wheels[1].surface_grip
        zero(v.physics[slot].wheels[1].surface_grip);
        // 2100..2103: physics.wheels[1].surface_roughness
        zero(v.physics[slot].wheels[1].surface_roughness);
        // 2104..2107: physics.wheels[1].surface_rolling
        zero(v.physics[slot].wheels[1].surface_rolling);
        // 2108..210B: wheels[1].previous_angle
        zero(v.wheels[slot][1].previous_angle);
        // 210C..210F: wheels[1].current_angle
        zero(v.wheels[slot][1].current_angle);
        // 2110..2113: physics.wheels[2].acceleration
        zero(v.physics[slot].wheels[2].acceleration);
        // 2114..2117: physics.wheels[2].speed
        zero(v.physics[slot].wheels[2].speed);
        // 2118..211B: physics.wheels[2].slip
        zero(v.physics[slot].wheels[2].slip);
        // 211C..211F: physics.wheels[2].load
        zero(v.physics[slot].wheels[2].load);
        // 2120..2123: physics.wheels[2].reference_load
        zero(v.physics[slot].wheels[2].reference_load);
        // 2124..2127: physics.wheels[2].lateral_speed
        zero(v.physics[slot].wheels[2].lateral_speed);
        // 2128..212B: particles.field_1ff8[2]
        zero(v.particles[slot].field_1ff8[2]);
        // 212C..212F: physics.wheels[2].rolling_force
        zero(v.physics[slot].wheels[2].rolling_force);
        // 2130..213F: source_uninterpreted.offset_2130
        zero(v.source_uninterpreted[slot].offset_2130);
        // 2140..2143: physics.wheels[2].grip_usage
        zero(v.physics[slot].wheels[2].grip_usage);
        // 2144..2147: physics.wheels[2].static_friction
        zero(v.physics[slot].wheels[2].static_friction);
        // 2148..214B: physics.wheels[2].kinetic_friction
        zero(v.physics[slot].wheels[2].kinetic_friction);
        // 214C..215B: presentation.wheel_centers[2]
        zero(v.presentation[slot].wheel_centers[2]);
        // 215C..216B: physics.wheels[2].contact_normal
        zero(v.physics[slot].wheels[2].contact_normal);
        // 216C..217F: source_uninterpreted.offset_216c
        zero(v.source_uninterpreted[slot].offset_216c);
        // 2180..2183: physics.wheels[2].contact_countdown
        zero(v.physics[slot].wheels[2].contact_countdown);
        // 2184..2187: contact.wheels[2].previous_type
        zero(v.contact[slot].wheels[2].previous_type);
        // 2188..218B: contact.wheels[2].type
        zero(v.contact[slot].wheels[2].type);
        // 218C..218F: contact.wheels[2].changed
        zero(v.contact[slot].wheels[2].changed);
        // 2190..2193: contact.wheels[2].current_load
        zero(v.contact[slot].wheels[2].current_load);
        // 2194..2197: physics.wheels[2].surface_grip
        zero(v.physics[slot].wheels[2].surface_grip);
        // 2198..219B: physics.wheels[2].surface_roughness
        zero(v.physics[slot].wheels[2].surface_roughness);
        // 219C..219F: physics.wheels[2].surface_rolling
        zero(v.physics[slot].wheels[2].surface_rolling);
        // 21A0..21A3: wheels[2].previous_angle
        zero(v.wheels[slot][2].previous_angle);
        // 21A4..21A7: wheels[2].current_angle
        zero(v.wheels[slot][2].current_angle);
        // 21A8..21AB: physics.wheels[3].acceleration
        zero(v.physics[slot].wheels[3].acceleration);
        // 21AC..21AF: physics.wheels[3].speed
        zero(v.physics[slot].wheels[3].speed);
        // 21B0..21B3: physics.wheels[3].slip
        zero(v.physics[slot].wheels[3].slip);
        // 21B4..21B7: physics.wheels[3].load
        zero(v.physics[slot].wheels[3].load);
        // 21B8..21BB: physics.wheels[3].reference_load
        zero(v.physics[slot].wheels[3].reference_load);
        // 21BC..21BF: physics.wheels[3].lateral_speed
        zero(v.physics[slot].wheels[3].lateral_speed);
        // 21C0..21C3: particles.field_1ff8[3]
        zero(v.particles[slot].field_1ff8[3]);
        // 21C4..21C7: physics.wheels[3].rolling_force
        zero(v.physics[slot].wheels[3].rolling_force);
        // 21C8..21D7: source_uninterpreted.offset_21c8
        zero(v.source_uninterpreted[slot].offset_21c8);
        // 21D8..21DB: physics.wheels[3].grip_usage
        zero(v.physics[slot].wheels[3].grip_usage);
        // 21DC..21DF: physics.wheels[3].static_friction
        zero(v.physics[slot].wheels[3].static_friction);
        // 21E0..21E3: physics.wheels[3].kinetic_friction
        zero(v.physics[slot].wheels[3].kinetic_friction);
        // 21E4..21F3: presentation.wheel_centers[3]
        zero(v.presentation[slot].wheel_centers[3]);
        // 21F4..2203: physics.wheels[3].contact_normal
        zero(v.physics[slot].wheels[3].contact_normal);
        // 2204..2217: source_uninterpreted.offset_2204
        zero(v.source_uninterpreted[slot].offset_2204);
        // 2218..221B: physics.wheels[3].contact_countdown
        zero(v.physics[slot].wheels[3].contact_countdown);
        // 221C..221F: contact.wheels[3].previous_type
        zero(v.contact[slot].wheels[3].previous_type);
        // 2220..2223: contact.wheels[3].type
        zero(v.contact[slot].wheels[3].type);
        // 2224..2227: contact.wheels[3].changed
        zero(v.contact[slot].wheels[3].changed);
        // 2228..222B: contact.wheels[3].current_load
        zero(v.contact[slot].wheels[3].current_load);
        // 222C..222F: physics.wheels[3].surface_grip
        zero(v.physics[slot].wheels[3].surface_grip);
        // 2230..2233: physics.wheels[3].surface_roughness
        zero(v.physics[slot].wheels[3].surface_roughness);
        // 2234..2237: physics.wheels[3].surface_rolling
        zero(v.physics[slot].wheels[3].surface_rolling);
        // 2238..223B: wheels[3].previous_angle
        zero(v.wheels[slot][3].previous_angle);
        // 223C..223F: wheels[3].current_angle
        zero(v.wheels[slot][3].current_angle);
        // 2240..224F: physics.suspension
        zero(v.physics[slot].suspension);
        // 2250..2253: physics.turbo_active
        zero(v.physics[slot].turbo_active);
        // 2254..2257: physics.turbo_rpm
        zero(v.physics[slot].turbo_rpm);
        // 2258..225B: physics.turbo_force
        zero(v.physics[slot].turbo_force);
        // 225C..225F: model_effects.phase
        zero(v.model_effects[slot].phase);
        // 2260..2263: physics.launch_effect_level
        zero(v.physics[slot].launch_effect_level);
        // 2264..2267: physics.launch_effect_strength
        zero(v.physics[slot].launch_effect_strength);
        // 2268..226B: physics.launch_effect_timer
        zero(v.physics[slot].launch_effect_timer);
        // 226C..226F: model_effects.enabled
        zero(v.model_effects[slot].enabled);
        // 2270..2273: physics.fuel
        zero(v.physics[slot].fuel);
        // 2274..2277: load.current_refuel_rate
        zero(v.load[slot].current_refuel_rate);
        // 2278..227B: race.control_enabled
        zero(v.race[slot].control_enabled);
        // 227C..227F: race.control_kind
        zero(v.race[slot].control_kind);
        // 2280..2283: startup.field_2280
        zero(v.startup[slot].field_2280);
        // 2284..2290: source_input.vehicle_profiles[slot]
        for(unsigned byte=0;byte!=12;++byte)zero(state.source_input.vehicle_profiles[slot][byte]);
        // DWORD 2290: profile[12], then sparse 2291/2292/2293 below.
        zero(state.source_input.vehicle_profiles[slot][12]);
        // 2291..2297: source_uninterpreted.offset_2291
        zero(v.source_uninterpreted[slot].offset_2291[0]);
        zero(v.source_uninterpreted[slot].offset_2291[1]);
        zero(v.source_uninterpreted[slot].offset_2291[2]);
        // DWORD 2294 remains in the same sparse owner.
        for(unsigned byte=3;byte!=7;++byte)zero(v.source_uninterpreted[slot].offset_2291[byte]);
        // 2298..229B: physics.steering_ramp
        zero(v.physics[slot].steering_ramp);
        // 229C..229F: physics.brake_ramp
        zero(v.physics[slot].brake_ramp);
        // 22A0..22A0: restart.previous_input.throttle
        // DWORD 22A0: four previous-input bytes in restart.
        zero(v.restart[slot].previous_input.throttle);
        // 22A1..22A1: restart.previous_input.brake
        zero(v.restart[slot].previous_input.brake);
        // 22A2..22A2: restart.previous_input.left
        zero(v.restart[slot].previous_input.left);
        // 22A3..22A3: restart.previous_input.right
        zero(v.restart[slot].previous_input.right);
        // 22A4..22A4: physics.previous_shift
        // DWORD 22A4: physics shift/stunt, then restart lock/boost.
        zero(v.physics[slot].previous_shift);
        // 22A5..22A5: physics.previous_stunt
        zero(v.physics[slot].previous_stunt);
        // 22A6..22A6: restart.previous_input.rear_lock
        zero(v.restart[slot].previous_input.rear_lock);
        // 22A7..22A7: restart.previous_input.steering_boost
        zero(v.restart[slot].previous_input.steering_boost);
        // 22A8..22A8: restart.previous_input.turbo
        // DWORD 22A8: restart turbo/aux, then current throttle/brake.
        zero(v.restart[slot].previous_input.turbo);
        // 22A9..22A9: restart.previous_input.auxiliary
        zero(v.restart[slot].previous_input.auxiliary);
        // 22AA..22B3: physics.input
        zero(v.physics[slot].input);
        // 22B4..22C3: physics.control_flags
        zero(v.physics[slot].control_flags);
        // 22C4..22C7: physics.steering_boost
        zero(v.physics[slot].steering_boost);
        // 22C8..22CB: race.saved_camera_mode
        zero(v.race[slot].saved_camera_mode);
        // 22CC..22DB: camera_settings.orbit_offset
        zero(v.camera_settings[slot].orbit_offset);
        // 22DC..22EB: camera_settings.target_offset
        zero(v.camera_settings[slot].target_offset);
        // 22EC..22FB: camera_settings.angles
        zero(v.camera_settings[slot].angles);
        // 22FC..22FF: camera_settings.distance
        zero(v.camera_settings[slot].distance);
        // 2300..2303: camera_settings.approach_scale
        zero(v.camera_settings[slot].approach_scale);
        // 2304..2307: camera_settings.adaptive_distance
        zero(v.camera_settings[slot].adaptive_distance);
        // 2308..2317: physics.auxiliary_displacement
        zero(v.physics[slot].auxiliary_displacement);
        // 2318..2327: physics.previous_auxiliary_displacement
        zero(v.physics[slot].previous_auxiliary_displacement);
        // 2328..2337: physics.auxiliary_velocity
        zero(v.physics[slot].auxiliary_velocity);
        // 2338..233B: audio.horn_handle
        zero(v.audio[slot].horn_handle);
        // 233C..234B: audio.fields_233c
        zero(v.audio[slot].fields_233c);
        // 234C..234F: audio.effect_hold
        zero(v.audio[slot].effect_hold);
        // 2350..2353: audio.field_2350
        zero(v.audio[slot].field_2350);
        // 2354..2357: audio.effect_pending
        zero(v.audio[slot].effect_pending);
        // 2358..235B: audio.field_2358
        zero(v.audio[slot].field_2358);
        // 235C..235F: audio.effect_strength
        zero(v.audio[slot].effect_strength);
        // 2360..2363: contact.shock
        zero(v.contact[slot].shock);
        // 2364..2367: contact.pair_shock
        zero(v.contact[slot].pair_shock);
        // 2368..236B: contact.effect_2368
        zero(v.contact[slot].effect_2368);
        // 236C..236F: contact.shock_hold
        zero(v.contact[slot].shock_hold);
        // 2370..2373: audio.impact_handle
        zero(v.audio[slot].impact_handle);
        // 2374..2377: audio.previous_wheel_touched
        zero(v.audio[slot].previous_wheel_touched);
        // 2378..237B: contact.wheel_touched
        zero(v.contact[slot].wheel_touched);
        // 237C..237F: audio.grip_handle
        zero(v.audio[slot].grip_handle);
        // 2380..2383: audio.scrape_handle
        zero(v.audio[slot].scrape_handle);
        // 2384..2387: audio.rolling_handle
        zero(v.audio[slot].rolling_handle);
        // 2388..238B: audio.boost_handle
        zero(v.audio[slot].boost_handle);
        // 238C..238F: audio.idle_handle
        zero(v.audio[slot].idle_handle);
        // 2390..2393: audio.rev_handle
        zero(v.audio[slot].rev_handle);
        // 2394..2397: physics.launch_event
        zero(v.physics[slot].launch_event);
        // 2398..239B: physics.launch_event_strength
        zero(v.physics[slot].launch_event_strength);
        // 239C..239F: audio.motion_handle
        zero(v.audio[slot].motion_handle);
        // 23A0..23A3: physics.gear_changed
        zero(v.physics[slot].gear_changed);
        // 23A4..23A7: race.race_index
        zero(v.race[slot].race_index);
        // 23A8..23AB: race.previous_progress
        zero(v.race[slot].previous_progress);
        // 23AC..23AF: race.current_progress
        zero(v.race[slot].current_progress);
        // 23B0..23B3: race.progress
        zero(v.race[slot].progress);
        // 23B4..23B7: progression.previous_projection
        zero(v.progression[slot].previous_projection);
        // 23B8..23BB: race.previous_fraction
        zero(v.race[slot].previous_fraction);
        // 23BC..23BF: race.lap_index
        zero(v.race[slot].lap_index);
        // 23C0..23C3: progression.record_improved
        zero(v.progression[slot].record_improved);
        // 23C4..23C7: race.lap_timer
        zero(v.race[slot].lap_timer);
        // 23C8..23CB: race.split_timer
        zero(v.race[slot].split_timer);
        // 23CC..23CF: race.race_timer
        zero(v.race[slot].race_timer);
        // 23D0..23D3: race.stunt_score
        zero(v.race[slot].stunt_score);
        // 23D4..23D7: race.best_lap_ticks
        zero(v.race[slot].best_lap_ticks);
        // 23D8..23DB: race.total_ticks
        zero(v.race[slot].total_ticks);
        // 23DC..243B: race.lap_times
        zero(v.race[slot].lap_times);
        // 243C..243F: progression.lap_time_24
        zero(v.progression[slot].lap_time_24);
        // 2440..24A3: progression.lap_scores
        zero(v.progression[slot].lap_scores);
        // 24A4..24A7: race.spline_index
        zero(v.race[slot].spline_index);
        // 24A8..24AB: race.progress_crossing
        zero(v.race[slot].progress_crossing);
        // 24AC..24AF: race.place
        zero(v.race[slot].place);
        // 24B0..24BF: race.respawn_position
        zero(v.race[slot].respawn_position);
        // 24C0..24CF: race.respawn_heading
        zero(v.race[slot].respawn_heading);
        // 24D0..24DF: race.route_right
        zero(v.race[slot].route_right);
        // 24E0..24E3: physics.energy_exempt
        zero(v.physics[slot].energy_exempt);
        // 24E4..24E7: race.finish_state
        zero(v.race[slot].finish_state);
        // 24E8..24EB: race.stopped
        zero(v.race[slot].stopped);
        // 24EC..24EF: progression.checkpoint_index
        zero(v.progression[slot].checkpoint_index);
        // 24F0..2517: progression.checkpoint_times
        zero(v.progression[slot].checkpoint_times);
        // 2518..253F: progression.personal_checkpoint_times
        zero(v.progression[slot].personal_checkpoint_times);
        // 2540..2567: progression.record_checkpoint_times
        zero(v.progression[slot].record_checkpoint_times);
        // 2568..256B: contact.impact_count.value
        zero(v.contact[slot].impact_count.value);
        // 256C..256F: contact.impact_count.capacity
        zero(v.contact[slot].impact_count.capacity);
        // 2570..2573: physics.energy
        zero(v.physics[slot].energy);
        // 2574..2577: physics.energy_capacity
        zero(v.physics[slot].energy_capacity);
        // 2578..257B: tick.low_energy_phase
        zero(v.tick[slot].low_energy_phase);
        // 257C..257F: contact.accumulated_energy_loss
        zero(v.contact[slot].accumulated_energy_loss);
        // 2580..2583: contact.energy_hold
        zero(v.contact[slot].energy_hold);
        // 2584..25C3: physics.stunt_statistics
        zero(v.physics[slot].stunt_statistics);
        // 25C4..25DB: contact.pair_counters.current
        zero(v.contact[slot].pair_counters.current);
        // 25DC..25F3: contact.pair_counters.previous
        zero(v.contact[slot].pair_counters.previous);
        // 25F4..260B: contact.pair_counters.count
        zero(v.contact[slot].pair_counters.count);
        // 260C..260F: physics.score
        zero(v.physics[slot].score);
        // 2610..2613: physics.air_score_rate
        zero(v.physics[slot].air_score_rate);
        // 2614..2617: physics.launch_score
        zero(v.physics[slot].launch_score);
        // 2618..261B: physics.effective_mass
        zero(v.physics[slot].effective_mass);
        // 261C..261F: physics.effective_com_height
        zero(v.physics[slot].effective_com_height);
        // 2620..2623: restart.mode_2620
        zero(v.restart[slot].mode_2620);
        // 2624..2627: contact.com_reset_ticks
        zero(v.contact[slot].com_reset_ticks);
        // 2628..262B: contact.com_2628
        zero(v.contact[slot].com_2628);
        // 262C..262F: contact.com_262c
        zero(v.contact[slot].com_262c);
        // 2630..2633: restart.mode_2630
        zero(v.restart[slot].mode_2630);
        // 2634..2637: physics.performance_multiplier
        zero(v.physics[slot].performance_multiplier);
        // 2638..263B: tick.performance_gap
        zero(v.tick[slot].performance_gap);
        // 263C..263F: source_uninterpreted.offset_263c
        zero(v.source_uninterpreted[slot].offset_263c);
        // 2640..2643: physics.upgrade_active
        zero(v.physics[slot].upgrade_active);
        // 2644..2647: race.route_indices[0]
        zero(v.race[slot].route_indices[0]);
        // 2648..264B: race.route_fractions[0]
        zero(v.race[slot].route_fractions[0]);
        // 264C..264F: race.route_indices[1]
        zero(v.race[slot].route_indices[1]);
        // 2650..2653: race.route_fractions[1]
        zero(v.race[slot].route_fractions[1]);
        // 2654..2657: race.route_indices[2]
        zero(v.race[slot].route_indices[2]);
        // 2658..265B: race.route_fractions[2]
        zero(v.race[slot].route_fractions[2]);
        // 265C..265F: route_assist.blend
        zero(v.route_assist[slot].blend);
        // 2660..2663: physics.upgrade_condition
        zero(v.physics[slot].upgrade_condition);
        // 2664..2667: ai.reverse_ticks
        zero(v.ai[slot].reverse_ticks);
        // 2668..266B: ai.bend
        zero(v.ai[slot].bend);
        // 266C..266F: ai.lateral_fraction
        zero(v.ai[slot].lateral_fraction);
        // 2670..2673: contact.pair_effect_ticks
        zero(v.contact[slot].pair_effect_ticks);
        // 2674..2677: ai.counter2674
        zero(v.ai[slot].counter2674);
        // 2678..267B: physics.stunt_meter
        zero(v.physics[slot].stunt_meter);
        // 267C..267F: ai_stunts.input_kind
        zero(v.ai_stunts[slot].input_kind);
        // 2680..2683: ai_stunts.grade
        zero(v.ai_stunts[slot].grade);
        // 2684..2687: ai_stunts.planned_route_distance
        zero(v.ai_stunts[slot].planned_route_distance);
        // 2688..268B: ai.passing_target
        zero(v.ai[slot].passing_target);
        // 268C..268F: ai.passing_ticks
        zero(v.ai[slot].passing_ticks);
        // 2690..2693: ai.passing_offset
        zero(v.ai[slot].passing_offset);
        // 2694..2697: ai.counter2694
        zero(v.ai[slot].counter2694);
        // 2698..269B: ai.turbo_cooldown
        zero(v.ai[slot].turbo_cooldown);
        // 269C..269F: ai.turbo_rest
        zero(v.ai[slot].turbo_rest);
        // 26A0..26A3: ai.following_ticks
        zero(v.ai[slot].following_ticks);
        // 26A4..26A7: ai.steering_ramp_ticks
        zero(v.ai[slot].steering_ramp_ticks);
        // 26A8..26AB: race.timing26a8
        zero(v.race[slot].timing26a8);
        // 26AC..26AF: ai.start_throttle_ticks
        zero(v.ai[slot].start_throttle_ticks);
        // 26B0..26B3: models.model
        zero(v.models[slot].model);
        // 26B4..26B7: models.shadow
        zero(v.models[slot].shadow);
        // 26B8..26C7: models.position
        zero(v.models[slot].position);
        // 26C8..26D7: models.scale
        zero(v.models[slot].scale);
        // 26D8..26E7: models.target
        zero(v.models[slot].target);
        // 26E8..26F7: models.up
        zero(v.models[slot].up);
        // 26F8..2707: models.template_tail
        zero(v.models[slot].template_tail);
        // 2708..270B: models.field_2708
        zero(v.models[slot].field_2708);
        // 270C..274B: models.body_rotation
        zero(v.models[slot].body_rotation);
        // 274C..278B: wheels[0].rotation
        zero(v.wheels[slot][0].rotation);
        // 278C..27CB: wheels[1].rotation
        zero(v.wheels[slot][1].rotation);
        // 27CC..280B: wheels[2].rotation
        zero(v.wheels[slot][2].rotation);
        // 280C..284B: wheels[3].rotation
        zero(v.wheels[slot][3].rotation);
        // 284C..285B: physics.front_axle_position
        zero(v.physics[slot].front_axle_position);
        // 285C..286B: physics.rear_axle_position
        zero(v.physics[slot].rear_axle_position);
        // 286C..287B: physics.front_axle_velocity
        zero(v.physics[slot].front_axle_velocity);
        // 287C..288B: physics.rear_axle_velocity
        zero(v.physics[slot].rear_axle_velocity);
        // 288C..288F: physics.body_heave
        zero(v.physics[slot].body_heave);
        // 2890..2893: physics.body_pitch
        zero(v.physics[slot].body_pitch);
        // 2894..2897: physics.body_roll
        zero(v.physics[slot].body_roll);
        // 2898..289B: physics.average_load
        zero(v.physics[slot].average_load);
        // 289C..289F: physics.average_grip
        zero(v.physics[slot].average_grip);
        // 28A0..28A3: trail_inputs.field_28a0
        zero(v.trail_inputs[slot].field_28a0);
        // 28A4..28A7: model_effects.cached_brake
        zero(v.model_effects[slot].cached_brake);
        // 28A8..28AB: physics.brake_signal
        zero(v.physics[slot].brake_signal);
        // 28AC..28AF: source_uninterpreted.offset_28ac
        zero(v.source_uninterpreted[slot].offset_28ac);
        // 28B0..28B3: initialization_extras.field_28b0
        zero(v.initialization_extras[slot].field_28b0);
        // 28B4..28BB: model_effects.glow_phases
        zero(v.model_effects[slot].glow_phases);
        // 28BC..28BF: trail_inputs.pair_b_available[0]
        zero(v.trail_inputs[slot].pair_b_available[0]);
        // 28C0..28CF: trail_inputs.pair_b[0]
        zero(v.trail_inputs[slot].pair_b[0]);
        // 28D0..2ACF: trails.pair_b[0]
        zero(v.trails[slot].pair_b[0]);
        // 2AD0..2AD3: trails.cursor_b[0]
        zero(v.trails[slot].cursor_b[0]);
        // 2AD4..2AD7: physics.camera_effects.values[6]
        zero(v.physics[slot].camera_effects.values[6]);
        // 2AD8..2ADB: trail_inputs.pair_b_available[1]
        zero(v.trail_inputs[slot].pair_b_available[1]);
        // 2ADC..2AEB: trail_inputs.pair_b[1]
        zero(v.trail_inputs[slot].pair_b[1]);
        // 2AEC..2CEB: trails.pair_b[1]
        zero(v.trails[slot].pair_b[1]);
        // 2CEC..2CEF: trails.cursor_b[1]
        zero(v.trails[slot].cursor_b[1]);
        // 2CF0..2CF3: physics.camera_effects.values[7]
        zero(v.physics[slot].camera_effects.values[7]);
        // 2CF4..2CF7: physics.wheels[0].braking_signal
        zero(v.physics[slot].wheels[0].braking_signal);
        // 2CF8..2CFB: physics.wheels[1].braking_signal
        zero(v.physics[slot].wheels[1].braking_signal);
        // 2CFC..2CFF: physics.wheels[2].braking_signal
        zero(v.physics[slot].wheels[2].braking_signal);
        // 2D00..2D03: physics.wheels[3].braking_signal
        zero(v.physics[slot].wheels[3].braking_signal);
        // 2D04..2D13: trail_inputs.pair_a[0]
        zero(v.trail_inputs[slot].pair_a[0]);
        // 2D14..2F13: trails.pair_a[0]
        zero(v.trails[slot].pair_a[0]);
        // 2F14..2F17: trails.cursor_a[0]
        zero(v.trails[slot].cursor_a[0]);
        // 2F18..2F1B: physics.camera_effects.values[4]
        zero(v.physics[slot].camera_effects.values[4]);
        // 2F1C..2F2B: trail_inputs.pair_a[1]
        zero(v.trail_inputs[slot].pair_a[1]);
        // 2F2C..312B: trails.pair_a[1]
        zero(v.trails[slot].pair_a[1]);
        // 312C..312F: trails.cursor_a[1]
        zero(v.trails[slot].cursor_a[1]);
        // 3130..3133: physics.camera_effects.values[5]
        zero(v.physics[slot].camera_effects.values[5]);
        // 3134..3137: presentation.field_3134
        zero(v.presentation[slot].field_3134);
        // 3138..3147: anchors.position
        zero(v.anchors[slot].position);
        // 3148..3157: anchors.forward
        zero(v.anchors[slot].forward);
        // 3158..315B: anchors.extent
        zero(v.anchors[slot].extent);
        // 315C..327B: segments.items
        zero(v.segments[slot].items);
        // 327C..328B: rail.effect_position
        zero(v.rail[slot].effect_position);
        // 328C..328F: anchors.effect_available[0]
        zero(v.anchors[slot].effect_available[0]);
        // 3290..329F: anchors.effect_position[0]
        zero(v.anchors[slot].effect_position[0]);
        // 32A0..32AF: anchors.effect_base_scale[0]
        zero(v.anchors[slot].effect_base_scale[0]);
        // 32B0..32BF: model_effects.current_scale[0]
        zero(v.model_effects[slot].current_scale[0]);
        // 32C0..33BF: model_effects.trails[0]
        zero(v.model_effects[slot].trails[0]);
        // 33C0..33C3: model_effects.cursor[0]
        zero(v.model_effects[slot].cursor[0]);
        // 33C4..33C7: physics.camera_effects.values[0]
        zero(v.physics[slot].camera_effects.values[0]);
        // 33C8..33CB: presentation.field_33c8
        zero(v.presentation[slot].field_33c8);
        // 33CC..33CF: anchors.effect_available[1]
        zero(v.anchors[slot].effect_available[1]);
        // 33D0..33DF: anchors.effect_position[1]
        zero(v.anchors[slot].effect_position[1]);
        // 33E0..33EF: anchors.effect_base_scale[1]
        zero(v.anchors[slot].effect_base_scale[1]);
        // 33F0..33FF: model_effects.current_scale[1]
        zero(v.model_effects[slot].current_scale[1]);
        // 3400..34FF: model_effects.trails[1]
        zero(v.model_effects[slot].trails[1]);
        // 3500..3503: model_effects.cursor[1]
        zero(v.model_effects[slot].cursor[1]);
        // 3504..3507: physics.camera_effects.values[1]
        zero(v.physics[slot].camera_effects.values[1]);
        // 3508..350B: presentation.field_3508
        zero(v.presentation[slot].field_3508);
        // 350C..351B: source_uninterpreted.offset_350c
        zero(v.source_uninterpreted[slot].offset_350c);
        // 351C..391B: trails.speed
        zero(v.trails[slot].speed);
        // 391C..391F: physics.camera_effects.values[2]
        zero(v.physics[slot].camera_effects.values[2]);
        // 3920..3923: physics.camera_effects.values[3]
        zero(v.physics[slot].camera_effects.values[3]);
        // 3924..3927: presentation.field_3924
        zero(v.presentation[slot].field_3924);
        // 3928..392B: presentation.field_3928
        zero(v.presentation[slot].field_3928);
        // 392C..392F: shadows.factor_392c
        zero(v.shadows[slot].factor_392c);
        // 3930..3933: shadows.factor_3930
        zero(v.shadows[slot].factor_3930);
        // 3934..3943: shadows.position
        zero(v.shadows[slot].position);
        // 3944..3953: shadows.normal
        zero(v.shadows[slot].normal);
        // 3954..3963: shadows.forward
        zero(v.shadows[slot].forward);
        // 3964..3967: strips.items[0].field_00
        zero(v.strips[slot].items[0].field_00);
        // 3968..396B: strips.items[0].field_04
        zero(v.strips[slot].items[0].field_04);
        // 396C..396F: strips.items[0].field_08
        zero(v.strips[slot].items[0].field_08);
        // 3970..3973: skid_rings.items[0].last_corner
        zero(v.skid_rings[slot].items[0].last_corner);
        // 3974..3983: skid_rings.items[0].segments[0].start
        zero(v.skid_rings[slot].items[0].segments[0].start);
        // 3984..3993: skid_rings.items[0].segments[0].end
        zero(v.skid_rings[slot].items[0].segments[0].end);
        // 3994..39A3: skid_rings.items[0].segments[0].step
        zero(v.skid_rings[slot].items[0].segments[0].step);
        // 39A4..39A7: skid_rings.items[0].segments[0].first_vertex
        zero(v.skid_rings[slot].items[0].segments[0].first_vertex);
        // 39A8..39B7: skid_rings.items[0].segments[1].start
        zero(v.skid_rings[slot].items[0].segments[1].start);
        // 39B8..39C7: skid_rings.items[0].segments[1].end
        zero(v.skid_rings[slot].items[0].segments[1].end);
        // 39C8..39D7: skid_rings.items[0].segments[1].step
        zero(v.skid_rings[slot].items[0].segments[1].step);
        // 39D8..39DB: skid_rings.items[0].segments[1].first_vertex
        zero(v.skid_rings[slot].items[0].segments[1].first_vertex);
        // 39DC..39EB: skid_rings.items[0].segments[2].start
        zero(v.skid_rings[slot].items[0].segments[2].start);
        // 39EC..39FB: skid_rings.items[0].segments[2].end
        zero(v.skid_rings[slot].items[0].segments[2].end);
        // 39FC..3A0B: skid_rings.items[0].segments[2].step
        zero(v.skid_rings[slot].items[0].segments[2].step);
        // 3A0C..3A0F: skid_rings.items[0].segments[2].first_vertex
        zero(v.skid_rings[slot].items[0].segments[2].first_vertex);
        // 3A10..3A1F: skid_rings.items[0].segments[3].start
        zero(v.skid_rings[slot].items[0].segments[3].start);
        // 3A20..3A2F: skid_rings.items[0].segments[3].end
        zero(v.skid_rings[slot].items[0].segments[3].end);
        // 3A30..3A3F: skid_rings.items[0].segments[3].step
        zero(v.skid_rings[slot].items[0].segments[3].step);
        // 3A40..3A43: skid_rings.items[0].segments[3].first_vertex
        zero(v.skid_rings[slot].items[0].segments[3].first_vertex);
        // 3A44..3A53: skid_rings.items[0].segments[4].start
        zero(v.skid_rings[slot].items[0].segments[4].start);
        // 3A54..3A63: skid_rings.items[0].segments[4].end
        zero(v.skid_rings[slot].items[0].segments[4].end);
        // 3A64..3A73: skid_rings.items[0].segments[4].step
        zero(v.skid_rings[slot].items[0].segments[4].step);
        // 3A74..3A77: skid_rings.items[0].segments[4].first_vertex
        zero(v.skid_rings[slot].items[0].segments[4].first_vertex);
        // 3A78..3A87: skid_rings.items[0].segments[5].start
        zero(v.skid_rings[slot].items[0].segments[5].start);
        // 3A88..3A97: skid_rings.items[0].segments[5].end
        zero(v.skid_rings[slot].items[0].segments[5].end);
        // 3A98..3AA7: skid_rings.items[0].segments[5].step
        zero(v.skid_rings[slot].items[0].segments[5].step);
        // 3AA8..3AAB: skid_rings.items[0].segments[5].first_vertex
        zero(v.skid_rings[slot].items[0].segments[5].first_vertex);
        // 3AAC..3ABB: skid_rings.items[0].segments[6].start
        zero(v.skid_rings[slot].items[0].segments[6].start);
        // 3ABC..3ACB: skid_rings.items[0].segments[6].end
        zero(v.skid_rings[slot].items[0].segments[6].end);
        // 3ACC..3ADB: skid_rings.items[0].segments[6].step
        zero(v.skid_rings[slot].items[0].segments[6].step);
        // 3ADC..3ADF: skid_rings.items[0].segments[6].first_vertex
        zero(v.skid_rings[slot].items[0].segments[6].first_vertex);
        // 3AE0..3AEF: skid_rings.items[0].segments[7].start
        zero(v.skid_rings[slot].items[0].segments[7].start);
        // 3AF0..3AFF: skid_rings.items[0].segments[7].end
        zero(v.skid_rings[slot].items[0].segments[7].end);
        // 3B00..3B0F: skid_rings.items[0].segments[7].step
        zero(v.skid_rings[slot].items[0].segments[7].step);
        // 3B10..3B13: skid_rings.items[0].segments[7].first_vertex
        zero(v.skid_rings[slot].items[0].segments[7].first_vertex);
        // 3B14..3B23: skid_rings.items[0].segments[8].start
        zero(v.skid_rings[slot].items[0].segments[8].start);
        // 3B24..3B33: skid_rings.items[0].segments[8].end
        zero(v.skid_rings[slot].items[0].segments[8].end);
        // 3B34..3B43: skid_rings.items[0].segments[8].step
        zero(v.skid_rings[slot].items[0].segments[8].step);
        // 3B44..3B47: skid_rings.items[0].segments[8].first_vertex
        zero(v.skid_rings[slot].items[0].segments[8].first_vertex);
        // 3B48..3B57: skid_rings.items[0].segments[9].start
        zero(v.skid_rings[slot].items[0].segments[9].start);
        // 3B58..3B67: skid_rings.items[0].segments[9].end
        zero(v.skid_rings[slot].items[0].segments[9].end);
        // 3B68..3B77: skid_rings.items[0].segments[9].step
        zero(v.skid_rings[slot].items[0].segments[9].step);
        // 3B78..3B7B: skid_rings.items[0].segments[9].first_vertex
        zero(v.skid_rings[slot].items[0].segments[9].first_vertex);
        // 3B7C..3B8B: skid_rings.items[0].segments[10].start
        zero(v.skid_rings[slot].items[0].segments[10].start);
        // 3B8C..3B9B: skid_rings.items[0].segments[10].end
        zero(v.skid_rings[slot].items[0].segments[10].end);
        // 3B9C..3BAB: skid_rings.items[0].segments[10].step
        zero(v.skid_rings[slot].items[0].segments[10].step);
        // 3BAC..3BAF: skid_rings.items[0].segments[10].first_vertex
        zero(v.skid_rings[slot].items[0].segments[10].first_vertex);
        // 3BB0..3BBF: skid_rings.items[0].segments[11].start
        zero(v.skid_rings[slot].items[0].segments[11].start);
        // 3BC0..3BCF: skid_rings.items[0].segments[11].end
        zero(v.skid_rings[slot].items[0].segments[11].end);
        // 3BD0..3BDF: skid_rings.items[0].segments[11].step
        zero(v.skid_rings[slot].items[0].segments[11].step);
        // 3BE0..3BE3: skid_rings.items[0].segments[11].first_vertex
        zero(v.skid_rings[slot].items[0].segments[11].first_vertex);
        // 3BE4..3BF3: skid_rings.items[0].segments[12].start
        zero(v.skid_rings[slot].items[0].segments[12].start);
        // 3BF4..3C03: skid_rings.items[0].segments[12].end
        zero(v.skid_rings[slot].items[0].segments[12].end);
        // 3C04..3C13: skid_rings.items[0].segments[12].step
        zero(v.skid_rings[slot].items[0].segments[12].step);
        // 3C14..3C17: skid_rings.items[0].segments[12].first_vertex
        zero(v.skid_rings[slot].items[0].segments[12].first_vertex);
        // 3C18..3C27: skid_rings.items[0].segments[13].start
        zero(v.skid_rings[slot].items[0].segments[13].start);
        // 3C28..3C37: skid_rings.items[0].segments[13].end
        zero(v.skid_rings[slot].items[0].segments[13].end);
        // 3C38..3C47: skid_rings.items[0].segments[13].step
        zero(v.skid_rings[slot].items[0].segments[13].step);
        // 3C48..3C4B: skid_rings.items[0].segments[13].first_vertex
        zero(v.skid_rings[slot].items[0].segments[13].first_vertex);
        // 3C4C..3C5B: skid_rings.items[0].segments[14].start
        zero(v.skid_rings[slot].items[0].segments[14].start);
        // 3C5C..3C6B: skid_rings.items[0].segments[14].end
        zero(v.skid_rings[slot].items[0].segments[14].end);
        // 3C6C..3C7B: skid_rings.items[0].segments[14].step
        zero(v.skid_rings[slot].items[0].segments[14].step);
        // 3C7C..3C7F: skid_rings.items[0].segments[14].first_vertex
        zero(v.skid_rings[slot].items[0].segments[14].first_vertex);
        // 3C80..3C8F: skid_rings.items[0].segments[15].start
        zero(v.skid_rings[slot].items[0].segments[15].start);
        // 3C90..3C9F: skid_rings.items[0].segments[15].end
        zero(v.skid_rings[slot].items[0].segments[15].end);
        // 3CA0..3CAF: skid_rings.items[0].segments[15].step
        zero(v.skid_rings[slot].items[0].segments[15].step);
        // 3CB0..3CB3: skid_rings.items[0].segments[15].first_vertex
        zero(v.skid_rings[slot].items[0].segments[15].first_vertex);
        // 3CB4..3CC3: skid_rings.items[0].segments[16].start
        zero(v.skid_rings[slot].items[0].segments[16].start);
        // 3CC4..3CD3: skid_rings.items[0].segments[16].end
        zero(v.skid_rings[slot].items[0].segments[16].end);
        // 3CD4..3CE3: skid_rings.items[0].segments[16].step
        zero(v.skid_rings[slot].items[0].segments[16].step);
        // 3CE4..3CE7: skid_rings.items[0].segments[16].first_vertex
        zero(v.skid_rings[slot].items[0].segments[16].first_vertex);
        // 3CE8..3CF7: skid_rings.items[0].segments[17].start
        zero(v.skid_rings[slot].items[0].segments[17].start);
        // 3CF8..3D07: skid_rings.items[0].segments[17].end
        zero(v.skid_rings[slot].items[0].segments[17].end);
        // 3D08..3D17: skid_rings.items[0].segments[17].step
        zero(v.skid_rings[slot].items[0].segments[17].step);
        // 3D18..3D1B: skid_rings.items[0].segments[17].first_vertex
        zero(v.skid_rings[slot].items[0].segments[17].first_vertex);
        // 3D1C..3D2B: skid_rings.items[0].segments[18].start
        zero(v.skid_rings[slot].items[0].segments[18].start);
        // 3D2C..3D3B: skid_rings.items[0].segments[18].end
        zero(v.skid_rings[slot].items[0].segments[18].end);
        // 3D3C..3D4B: skid_rings.items[0].segments[18].step
        zero(v.skid_rings[slot].items[0].segments[18].step);
        // 3D4C..3D4F: skid_rings.items[0].segments[18].first_vertex
        zero(v.skid_rings[slot].items[0].segments[18].first_vertex);
        // 3D50..3D5F: skid_rings.items[0].segments[19].start
        zero(v.skid_rings[slot].items[0].segments[19].start);
        // 3D60..3D6F: skid_rings.items[0].segments[19].end
        zero(v.skid_rings[slot].items[0].segments[19].end);
        // 3D70..3D7F: skid_rings.items[0].segments[19].step
        zero(v.skid_rings[slot].items[0].segments[19].step);
        // 3D80..3D83: skid_rings.items[0].segments[19].first_vertex
        zero(v.skid_rings[slot].items[0].segments[19].first_vertex);
        // 3D84..3D93: skid_rings.items[0].segments[20].start
        zero(v.skid_rings[slot].items[0].segments[20].start);
        // 3D94..3DA3: skid_rings.items[0].segments[20].end
        zero(v.skid_rings[slot].items[0].segments[20].end);
        // 3DA4..3DB3: skid_rings.items[0].segments[20].step
        zero(v.skid_rings[slot].items[0].segments[20].step);
        // 3DB4..3DB7: skid_rings.items[0].segments[20].first_vertex
        zero(v.skid_rings[slot].items[0].segments[20].first_vertex);
        // 3DB8..3DC7: skid_rings.items[0].segments[21].start
        zero(v.skid_rings[slot].items[0].segments[21].start);
        // 3DC8..3DD7: skid_rings.items[0].segments[21].end
        zero(v.skid_rings[slot].items[0].segments[21].end);
        // 3DD8..3DE7: skid_rings.items[0].segments[21].step
        zero(v.skid_rings[slot].items[0].segments[21].step);
        // 3DE8..3DEB: skid_rings.items[0].segments[21].first_vertex
        zero(v.skid_rings[slot].items[0].segments[21].first_vertex);
        // 3DEC..3DFB: skid_rings.items[0].segments[22].start
        zero(v.skid_rings[slot].items[0].segments[22].start);
        // 3DFC..3E0B: skid_rings.items[0].segments[22].end
        zero(v.skid_rings[slot].items[0].segments[22].end);
        // 3E0C..3E1B: skid_rings.items[0].segments[22].step
        zero(v.skid_rings[slot].items[0].segments[22].step);
        // 3E1C..3E1F: skid_rings.items[0].segments[22].first_vertex
        zero(v.skid_rings[slot].items[0].segments[22].first_vertex);
        // 3E20..3E2F: skid_rings.items[0].segments[23].start
        zero(v.skid_rings[slot].items[0].segments[23].start);
        // 3E30..3E3F: skid_rings.items[0].segments[23].end
        zero(v.skid_rings[slot].items[0].segments[23].end);
        // 3E40..3E4F: skid_rings.items[0].segments[23].step
        zero(v.skid_rings[slot].items[0].segments[23].step);
        // 3E50..3E53: skid_rings.items[0].segments[23].first_vertex
        zero(v.skid_rings[slot].items[0].segments[23].first_vertex);
        // 3E54..3E63: skid_rings.items[0].segments[24].start
        zero(v.skid_rings[slot].items[0].segments[24].start);
        // 3E64..3E73: skid_rings.items[0].segments[24].end
        zero(v.skid_rings[slot].items[0].segments[24].end);
        // 3E74..3E83: skid_rings.items[0].segments[24].step
        zero(v.skid_rings[slot].items[0].segments[24].step);
        // 3E84..3E87: skid_rings.items[0].segments[24].first_vertex
        zero(v.skid_rings[slot].items[0].segments[24].first_vertex);
        // 3E88..3E97: skid_rings.items[0].segments[25].start
        zero(v.skid_rings[slot].items[0].segments[25].start);
        // 3E98..3EA7: skid_rings.items[0].segments[25].end
        zero(v.skid_rings[slot].items[0].segments[25].end);
        // 3EA8..3EB7: skid_rings.items[0].segments[25].step
        zero(v.skid_rings[slot].items[0].segments[25].step);
        // 3EB8..3EBB: skid_rings.items[0].segments[25].first_vertex
        zero(v.skid_rings[slot].items[0].segments[25].first_vertex);
        // 3EBC..3ECB: skid_rings.items[0].segments[26].start
        zero(v.skid_rings[slot].items[0].segments[26].start);
        // 3ECC..3EDB: skid_rings.items[0].segments[26].end
        zero(v.skid_rings[slot].items[0].segments[26].end);
        // 3EDC..3EEB: skid_rings.items[0].segments[26].step
        zero(v.skid_rings[slot].items[0].segments[26].step);
        // 3EEC..3EEF: skid_rings.items[0].segments[26].first_vertex
        zero(v.skid_rings[slot].items[0].segments[26].first_vertex);
        // 3EF0..3EFF: skid_rings.items[0].segments[27].start
        zero(v.skid_rings[slot].items[0].segments[27].start);
        // 3F00..3F0F: skid_rings.items[0].segments[27].end
        zero(v.skid_rings[slot].items[0].segments[27].end);
        // 3F10..3F1F: skid_rings.items[0].segments[27].step
        zero(v.skid_rings[slot].items[0].segments[27].step);
        // 3F20..3F23: skid_rings.items[0].segments[27].first_vertex
        zero(v.skid_rings[slot].items[0].segments[27].first_vertex);
        // 3F24..3F33: skid_rings.items[0].segments[28].start
        zero(v.skid_rings[slot].items[0].segments[28].start);
        // 3F34..3F43: skid_rings.items[0].segments[28].end
        zero(v.skid_rings[slot].items[0].segments[28].end);
        // 3F44..3F53: skid_rings.items[0].segments[28].step
        zero(v.skid_rings[slot].items[0].segments[28].step);
        // 3F54..3F57: skid_rings.items[0].segments[28].first_vertex
        zero(v.skid_rings[slot].items[0].segments[28].first_vertex);
        // 3F58..3F67: skid_rings.items[0].segments[29].start
        zero(v.skid_rings[slot].items[0].segments[29].start);
        // 3F68..3F77: skid_rings.items[0].segments[29].end
        zero(v.skid_rings[slot].items[0].segments[29].end);
        // 3F78..3F87: skid_rings.items[0].segments[29].step
        zero(v.skid_rings[slot].items[0].segments[29].step);
        // 3F88..3F8B: skid_rings.items[0].segments[29].first_vertex
        zero(v.skid_rings[slot].items[0].segments[29].first_vertex);
        // 3F8C..3F9B: skid_rings.items[0].segments[30].start
        zero(v.skid_rings[slot].items[0].segments[30].start);
        // 3F9C..3FAB: skid_rings.items[0].segments[30].end
        zero(v.skid_rings[slot].items[0].segments[30].end);
        // 3FAC..3FBB: skid_rings.items[0].segments[30].step
        zero(v.skid_rings[slot].items[0].segments[30].step);
        // 3FBC..3FBF: skid_rings.items[0].segments[30].first_vertex
        zero(v.skid_rings[slot].items[0].segments[30].first_vertex);
        // 3FC0..3FCF: skid_rings.items[0].segments[31].start
        zero(v.skid_rings[slot].items[0].segments[31].start);
        // 3FD0..3FDF: skid_rings.items[0].segments[31].end
        zero(v.skid_rings[slot].items[0].segments[31].end);
        // 3FE0..3FEF: skid_rings.items[0].segments[31].step
        zero(v.skid_rings[slot].items[0].segments[31].step);
        // 3FF0..3FF3: skid_rings.items[0].segments[31].first_vertex
        zero(v.skid_rings[slot].items[0].segments[31].first_vertex);
        // 3FF4..4003: skid_rings.items[0].segments[32].start
        zero(v.skid_rings[slot].items[0].segments[32].start);
        // 4004..4013: skid_rings.items[0].segments[32].end
        zero(v.skid_rings[slot].items[0].segments[32].end);
        // 4014..4023: skid_rings.items[0].segments[32].step
        zero(v.skid_rings[slot].items[0].segments[32].step);
        // 4024..4027: skid_rings.items[0].segments[32].first_vertex
        zero(v.skid_rings[slot].items[0].segments[32].first_vertex);
        // 4028..4037: skid_rings.items[0].segments[33].start
        zero(v.skid_rings[slot].items[0].segments[33].start);
        // 4038..4047: skid_rings.items[0].segments[33].end
        zero(v.skid_rings[slot].items[0].segments[33].end);
        // 4048..4057: skid_rings.items[0].segments[33].step
        zero(v.skid_rings[slot].items[0].segments[33].step);
        // 4058..405B: skid_rings.items[0].segments[33].first_vertex
        zero(v.skid_rings[slot].items[0].segments[33].first_vertex);
        // 405C..406B: skid_rings.items[0].segments[34].start
        zero(v.skid_rings[slot].items[0].segments[34].start);
        // 406C..407B: skid_rings.items[0].segments[34].end
        zero(v.skid_rings[slot].items[0].segments[34].end);
        // 407C..408B: skid_rings.items[0].segments[34].step
        zero(v.skid_rings[slot].items[0].segments[34].step);
        // 408C..408F: skid_rings.items[0].segments[34].first_vertex
        zero(v.skid_rings[slot].items[0].segments[34].first_vertex);
        // 4090..409F: skid_rings.items[0].segments[35].start
        zero(v.skid_rings[slot].items[0].segments[35].start);
        // 40A0..40AF: skid_rings.items[0].segments[35].end
        zero(v.skid_rings[slot].items[0].segments[35].end);
        // 40B0..40BF: skid_rings.items[0].segments[35].step
        zero(v.skid_rings[slot].items[0].segments[35].step);
        // 40C0..40C3: skid_rings.items[0].segments[35].first_vertex
        zero(v.skid_rings[slot].items[0].segments[35].first_vertex);
        // 40C4..40D3: skid_rings.items[0].segments[36].start
        zero(v.skid_rings[slot].items[0].segments[36].start);
        // 40D4..40E3: skid_rings.items[0].segments[36].end
        zero(v.skid_rings[slot].items[0].segments[36].end);
        // 40E4..40F3: skid_rings.items[0].segments[36].step
        zero(v.skid_rings[slot].items[0].segments[36].step);
        // 40F4..40F7: skid_rings.items[0].segments[36].first_vertex
        zero(v.skid_rings[slot].items[0].segments[36].first_vertex);
        // 40F8..4107: skid_rings.items[0].segments[37].start
        zero(v.skid_rings[slot].items[0].segments[37].start);
        // 4108..4117: skid_rings.items[0].segments[37].end
        zero(v.skid_rings[slot].items[0].segments[37].end);
        // 4118..4127: skid_rings.items[0].segments[37].step
        zero(v.skid_rings[slot].items[0].segments[37].step);
        // 4128..412B: skid_rings.items[0].segments[37].first_vertex
        zero(v.skid_rings[slot].items[0].segments[37].first_vertex);
        // 412C..413B: skid_rings.items[0].segments[38].start
        zero(v.skid_rings[slot].items[0].segments[38].start);
        // 413C..414B: skid_rings.items[0].segments[38].end
        zero(v.skid_rings[slot].items[0].segments[38].end);
        // 414C..415B: skid_rings.items[0].segments[38].step
        zero(v.skid_rings[slot].items[0].segments[38].step);
        // 415C..415F: skid_rings.items[0].segments[38].first_vertex
        zero(v.skid_rings[slot].items[0].segments[38].first_vertex);
        // 4160..416F: skid_rings.items[0].segments[39].start
        zero(v.skid_rings[slot].items[0].segments[39].start);
        // 4170..417F: skid_rings.items[0].segments[39].end
        zero(v.skid_rings[slot].items[0].segments[39].end);
        // 4180..418F: skid_rings.items[0].segments[39].step
        zero(v.skid_rings[slot].items[0].segments[39].step);
        // 4190..4193: skid_rings.items[0].segments[39].first_vertex
        zero(v.skid_rings[slot].items[0].segments[39].first_vertex);
        // 4194..41A3: skid_rings.items[0].segments[40].start
        zero(v.skid_rings[slot].items[0].segments[40].start);
        // 41A4..41B3: skid_rings.items[0].segments[40].end
        zero(v.skid_rings[slot].items[0].segments[40].end);
        // 41B4..41C3: skid_rings.items[0].segments[40].step
        zero(v.skid_rings[slot].items[0].segments[40].step);
        // 41C4..41C7: skid_rings.items[0].segments[40].first_vertex
        zero(v.skid_rings[slot].items[0].segments[40].first_vertex);
        // 41C8..41D7: skid_rings.items[0].segments[41].start
        zero(v.skid_rings[slot].items[0].segments[41].start);
        // 41D8..41E7: skid_rings.items[0].segments[41].end
        zero(v.skid_rings[slot].items[0].segments[41].end);
        // 41E8..41F7: skid_rings.items[0].segments[41].step
        zero(v.skid_rings[slot].items[0].segments[41].step);
        // 41F8..41FB: skid_rings.items[0].segments[41].first_vertex
        zero(v.skid_rings[slot].items[0].segments[41].first_vertex);
        // 41FC..420B: skid_rings.items[0].segments[42].start
        zero(v.skid_rings[slot].items[0].segments[42].start);
        // 420C..421B: skid_rings.items[0].segments[42].end
        zero(v.skid_rings[slot].items[0].segments[42].end);
        // 421C..422B: skid_rings.items[0].segments[42].step
        zero(v.skid_rings[slot].items[0].segments[42].step);
        // 422C..422F: skid_rings.items[0].segments[42].first_vertex
        zero(v.skid_rings[slot].items[0].segments[42].first_vertex);
        // 4230..423F: skid_rings.items[0].segments[43].start
        zero(v.skid_rings[slot].items[0].segments[43].start);
        // 4240..424F: skid_rings.items[0].segments[43].end
        zero(v.skid_rings[slot].items[0].segments[43].end);
        // 4250..425F: skid_rings.items[0].segments[43].step
        zero(v.skid_rings[slot].items[0].segments[43].step);
        // 4260..4263: skid_rings.items[0].segments[43].first_vertex
        zero(v.skid_rings[slot].items[0].segments[43].first_vertex);
        // 4264..4273: skid_rings.items[0].segments[44].start
        zero(v.skid_rings[slot].items[0].segments[44].start);
        // 4274..4283: skid_rings.items[0].segments[44].end
        zero(v.skid_rings[slot].items[0].segments[44].end);
        // 4284..4293: skid_rings.items[0].segments[44].step
        zero(v.skid_rings[slot].items[0].segments[44].step);
        // 4294..4297: skid_rings.items[0].segments[44].first_vertex
        zero(v.skid_rings[slot].items[0].segments[44].first_vertex);
        // 4298..42A7: skid_rings.items[0].segments[45].start
        zero(v.skid_rings[slot].items[0].segments[45].start);
        // 42A8..42B7: skid_rings.items[0].segments[45].end
        zero(v.skid_rings[slot].items[0].segments[45].end);
        // 42B8..42C7: skid_rings.items[0].segments[45].step
        zero(v.skid_rings[slot].items[0].segments[45].step);
        // 42C8..42CB: skid_rings.items[0].segments[45].first_vertex
        zero(v.skid_rings[slot].items[0].segments[45].first_vertex);
        // 42CC..42DB: skid_rings.items[0].segments[46].start
        zero(v.skid_rings[slot].items[0].segments[46].start);
        // 42DC..42EB: skid_rings.items[0].segments[46].end
        zero(v.skid_rings[slot].items[0].segments[46].end);
        // 42EC..42FB: skid_rings.items[0].segments[46].step
        zero(v.skid_rings[slot].items[0].segments[46].step);
        // 42FC..42FF: skid_rings.items[0].segments[46].first_vertex
        zero(v.skid_rings[slot].items[0].segments[46].first_vertex);
        // 4300..430F: skid_rings.items[0].segments[47].start
        zero(v.skid_rings[slot].items[0].segments[47].start);
        // 4310..431F: skid_rings.items[0].segments[47].end
        zero(v.skid_rings[slot].items[0].segments[47].end);
        // 4320..432F: skid_rings.items[0].segments[47].step
        zero(v.skid_rings[slot].items[0].segments[47].step);
        // 4330..4333: skid_rings.items[0].segments[47].first_vertex
        zero(v.skid_rings[slot].items[0].segments[47].first_vertex);
        // 4334..4343: skid_rings.items[0].segments[48].start
        zero(v.skid_rings[slot].items[0].segments[48].start);
        // 4344..4353: skid_rings.items[0].segments[48].end
        zero(v.skid_rings[slot].items[0].segments[48].end);
        // 4354..4363: skid_rings.items[0].segments[48].step
        zero(v.skid_rings[slot].items[0].segments[48].step);
        // 4364..4367: skid_rings.items[0].segments[48].first_vertex
        zero(v.skid_rings[slot].items[0].segments[48].first_vertex);
        // 4368..4377: skid_rings.items[0].segments[49].start
        zero(v.skid_rings[slot].items[0].segments[49].start);
        // 4378..4387: skid_rings.items[0].segments[49].end
        zero(v.skid_rings[slot].items[0].segments[49].end);
        // 4388..4397: skid_rings.items[0].segments[49].step
        zero(v.skid_rings[slot].items[0].segments[49].step);
        // 4398..439B: skid_rings.items[0].segments[49].first_vertex
        zero(v.skid_rings[slot].items[0].segments[49].first_vertex);
        // 439C..43AB: skid_rings.items[0].segments[50].start
        zero(v.skid_rings[slot].items[0].segments[50].start);
        // 43AC..43BB: skid_rings.items[0].segments[50].end
        zero(v.skid_rings[slot].items[0].segments[50].end);
        // 43BC..43CB: skid_rings.items[0].segments[50].step
        zero(v.skid_rings[slot].items[0].segments[50].step);
        // 43CC..43CF: skid_rings.items[0].segments[50].first_vertex
        zero(v.skid_rings[slot].items[0].segments[50].first_vertex);
        // 43D0..43DF: skid_rings.items[0].segments[51].start
        zero(v.skid_rings[slot].items[0].segments[51].start);
        // 43E0..43EF: skid_rings.items[0].segments[51].end
        zero(v.skid_rings[slot].items[0].segments[51].end);
        // 43F0..43FF: skid_rings.items[0].segments[51].step
        zero(v.skid_rings[slot].items[0].segments[51].step);
        // 4400..4403: skid_rings.items[0].segments[51].first_vertex
        zero(v.skid_rings[slot].items[0].segments[51].first_vertex);
        // 4404..4413: skid_rings.items[0].segments[52].start
        zero(v.skid_rings[slot].items[0].segments[52].start);
        // 4414..4423: skid_rings.items[0].segments[52].end
        zero(v.skid_rings[slot].items[0].segments[52].end);
        // 4424..4433: skid_rings.items[0].segments[52].step
        zero(v.skid_rings[slot].items[0].segments[52].step);
        // 4434..4437: skid_rings.items[0].segments[52].first_vertex
        zero(v.skid_rings[slot].items[0].segments[52].first_vertex);
        // 4438..4447: skid_rings.items[0].segments[53].start
        zero(v.skid_rings[slot].items[0].segments[53].start);
        // 4448..4457: skid_rings.items[0].segments[53].end
        zero(v.skid_rings[slot].items[0].segments[53].end);
        // 4458..4467: skid_rings.items[0].segments[53].step
        zero(v.skid_rings[slot].items[0].segments[53].step);
        // 4468..446B: skid_rings.items[0].segments[53].first_vertex
        zero(v.skid_rings[slot].items[0].segments[53].first_vertex);
        // 446C..447B: skid_rings.items[0].segments[54].start
        zero(v.skid_rings[slot].items[0].segments[54].start);
        // 447C..448B: skid_rings.items[0].segments[54].end
        zero(v.skid_rings[slot].items[0].segments[54].end);
        // 448C..449B: skid_rings.items[0].segments[54].step
        zero(v.skid_rings[slot].items[0].segments[54].step);
        // 449C..449F: skid_rings.items[0].segments[54].first_vertex
        zero(v.skid_rings[slot].items[0].segments[54].first_vertex);
        // 44A0..44AF: skid_rings.items[0].segments[55].start
        zero(v.skid_rings[slot].items[0].segments[55].start);
        // 44B0..44BF: skid_rings.items[0].segments[55].end
        zero(v.skid_rings[slot].items[0].segments[55].end);
        // 44C0..44CF: skid_rings.items[0].segments[55].step
        zero(v.skid_rings[slot].items[0].segments[55].step);
        // 44D0..44D3: skid_rings.items[0].segments[55].first_vertex
        zero(v.skid_rings[slot].items[0].segments[55].first_vertex);
        // 44D4..44E3: skid_rings.items[0].segments[56].start
        zero(v.skid_rings[slot].items[0].segments[56].start);
        // 44E4..44F3: skid_rings.items[0].segments[56].end
        zero(v.skid_rings[slot].items[0].segments[56].end);
        // 44F4..4503: skid_rings.items[0].segments[56].step
        zero(v.skid_rings[slot].items[0].segments[56].step);
        // 4504..4507: skid_rings.items[0].segments[56].first_vertex
        zero(v.skid_rings[slot].items[0].segments[56].first_vertex);
        // 4508..4517: skid_rings.items[0].segments[57].start
        zero(v.skid_rings[slot].items[0].segments[57].start);
        // 4518..4527: skid_rings.items[0].segments[57].end
        zero(v.skid_rings[slot].items[0].segments[57].end);
        // 4528..4537: skid_rings.items[0].segments[57].step
        zero(v.skid_rings[slot].items[0].segments[57].step);
        // 4538..453B: skid_rings.items[0].segments[57].first_vertex
        zero(v.skid_rings[slot].items[0].segments[57].first_vertex);
        // 453C..454B: skid_rings.items[0].segments[58].start
        zero(v.skid_rings[slot].items[0].segments[58].start);
        // 454C..455B: skid_rings.items[0].segments[58].end
        zero(v.skid_rings[slot].items[0].segments[58].end);
        // 455C..456B: skid_rings.items[0].segments[58].step
        zero(v.skid_rings[slot].items[0].segments[58].step);
        // 456C..456F: skid_rings.items[0].segments[58].first_vertex
        zero(v.skid_rings[slot].items[0].segments[58].first_vertex);
        // 4570..457F: skid_rings.items[0].segments[59].start
        zero(v.skid_rings[slot].items[0].segments[59].start);
        // 4580..458F: skid_rings.items[0].segments[59].end
        zero(v.skid_rings[slot].items[0].segments[59].end);
        // 4590..459F: skid_rings.items[0].segments[59].step
        zero(v.skid_rings[slot].items[0].segments[59].step);
        // 45A0..45A3: skid_rings.items[0].segments[59].first_vertex
        zero(v.skid_rings[slot].items[0].segments[59].first_vertex);
        // 45A4..45B3: skid_rings.items[0].segments[60].start
        zero(v.skid_rings[slot].items[0].segments[60].start);
        // 45B4..45C3: skid_rings.items[0].segments[60].end
        zero(v.skid_rings[slot].items[0].segments[60].end);
        // 45C4..45D3: skid_rings.items[0].segments[60].step
        zero(v.skid_rings[slot].items[0].segments[60].step);
        // 45D4..45D7: skid_rings.items[0].segments[60].first_vertex
        zero(v.skid_rings[slot].items[0].segments[60].first_vertex);
        // 45D8..45E7: skid_rings.items[0].segments[61].start
        zero(v.skid_rings[slot].items[0].segments[61].start);
        // 45E8..45F7: skid_rings.items[0].segments[61].end
        zero(v.skid_rings[slot].items[0].segments[61].end);
        // 45F8..4607: skid_rings.items[0].segments[61].step
        zero(v.skid_rings[slot].items[0].segments[61].step);
        // 4608..460B: skid_rings.items[0].segments[61].first_vertex
        zero(v.skid_rings[slot].items[0].segments[61].first_vertex);
        // 460C..461B: skid_rings.items[0].segments[62].start
        zero(v.skid_rings[slot].items[0].segments[62].start);
        // 461C..462B: skid_rings.items[0].segments[62].end
        zero(v.skid_rings[slot].items[0].segments[62].end);
        // 462C..463B: skid_rings.items[0].segments[62].step
        zero(v.skid_rings[slot].items[0].segments[62].step);
        // 463C..463F: skid_rings.items[0].segments[62].first_vertex
        zero(v.skid_rings[slot].items[0].segments[62].first_vertex);
        // 4640..464F: skid_rings.items[0].segments[63].start
        zero(v.skid_rings[slot].items[0].segments[63].start);
        // 4650..465F: skid_rings.items[0].segments[63].end
        zero(v.skid_rings[slot].items[0].segments[63].end);
        // 4660..466F: skid_rings.items[0].segments[63].step
        zero(v.skid_rings[slot].items[0].segments[63].step);
        // 4670..4673: skid_rings.items[0].segments[63].first_vertex
        zero(v.skid_rings[slot].items[0].segments[63].first_vertex);
        // 4674..4677: strips.items[1].field_00
        zero(v.strips[slot].items[1].field_00);
        // 4678..467B: strips.items[1].field_04
        zero(v.strips[slot].items[1].field_04);
        // 467C..467F: strips.items[1].field_08
        zero(v.strips[slot].items[1].field_08);
        // 4680..4683: skid_rings.items[1].last_corner
        zero(v.skid_rings[slot].items[1].last_corner);
        // 4684..4693: skid_rings.items[1].segments[0].start
        zero(v.skid_rings[slot].items[1].segments[0].start);
        // 4694..46A3: skid_rings.items[1].segments[0].end
        zero(v.skid_rings[slot].items[1].segments[0].end);
        // 46A4..46B3: skid_rings.items[1].segments[0].step
        zero(v.skid_rings[slot].items[1].segments[0].step);
        // 46B4..46B7: skid_rings.items[1].segments[0].first_vertex
        zero(v.skid_rings[slot].items[1].segments[0].first_vertex);
        // 46B8..46C7: skid_rings.items[1].segments[1].start
        zero(v.skid_rings[slot].items[1].segments[1].start);
        // 46C8..46D7: skid_rings.items[1].segments[1].end
        zero(v.skid_rings[slot].items[1].segments[1].end);
        // 46D8..46E7: skid_rings.items[1].segments[1].step
        zero(v.skid_rings[slot].items[1].segments[1].step);
        // 46E8..46EB: skid_rings.items[1].segments[1].first_vertex
        zero(v.skid_rings[slot].items[1].segments[1].first_vertex);
        // 46EC..46FB: skid_rings.items[1].segments[2].start
        zero(v.skid_rings[slot].items[1].segments[2].start);
        // 46FC..470B: skid_rings.items[1].segments[2].end
        zero(v.skid_rings[slot].items[1].segments[2].end);
        // 470C..471B: skid_rings.items[1].segments[2].step
        zero(v.skid_rings[slot].items[1].segments[2].step);
        // 471C..471F: skid_rings.items[1].segments[2].first_vertex
        zero(v.skid_rings[slot].items[1].segments[2].first_vertex);
        // 4720..472F: skid_rings.items[1].segments[3].start
        zero(v.skid_rings[slot].items[1].segments[3].start);
        // 4730..473F: skid_rings.items[1].segments[3].end
        zero(v.skid_rings[slot].items[1].segments[3].end);
        // 4740..474F: skid_rings.items[1].segments[3].step
        zero(v.skid_rings[slot].items[1].segments[3].step);
        // 4750..4753: skid_rings.items[1].segments[3].first_vertex
        zero(v.skid_rings[slot].items[1].segments[3].first_vertex);
        // 4754..4763: skid_rings.items[1].segments[4].start
        zero(v.skid_rings[slot].items[1].segments[4].start);
        // 4764..4773: skid_rings.items[1].segments[4].end
        zero(v.skid_rings[slot].items[1].segments[4].end);
        // 4774..4783: skid_rings.items[1].segments[4].step
        zero(v.skid_rings[slot].items[1].segments[4].step);
        // 4784..4787: skid_rings.items[1].segments[4].first_vertex
        zero(v.skid_rings[slot].items[1].segments[4].first_vertex);
        // 4788..4797: skid_rings.items[1].segments[5].start
        zero(v.skid_rings[slot].items[1].segments[5].start);
        // 4798..47A7: skid_rings.items[1].segments[5].end
        zero(v.skid_rings[slot].items[1].segments[5].end);
        // 47A8..47B7: skid_rings.items[1].segments[5].step
        zero(v.skid_rings[slot].items[1].segments[5].step);
        // 47B8..47BB: skid_rings.items[1].segments[5].first_vertex
        zero(v.skid_rings[slot].items[1].segments[5].first_vertex);
        // 47BC..47CB: skid_rings.items[1].segments[6].start
        zero(v.skid_rings[slot].items[1].segments[6].start);
        // 47CC..47DB: skid_rings.items[1].segments[6].end
        zero(v.skid_rings[slot].items[1].segments[6].end);
        // 47DC..47EB: skid_rings.items[1].segments[6].step
        zero(v.skid_rings[slot].items[1].segments[6].step);
        // 47EC..47EF: skid_rings.items[1].segments[6].first_vertex
        zero(v.skid_rings[slot].items[1].segments[6].first_vertex);
        // 47F0..47FF: skid_rings.items[1].segments[7].start
        zero(v.skid_rings[slot].items[1].segments[7].start);
        // 4800..480F: skid_rings.items[1].segments[7].end
        zero(v.skid_rings[slot].items[1].segments[7].end);
        // 4810..481F: skid_rings.items[1].segments[7].step
        zero(v.skid_rings[slot].items[1].segments[7].step);
        // 4820..4823: skid_rings.items[1].segments[7].first_vertex
        zero(v.skid_rings[slot].items[1].segments[7].first_vertex);
        // 4824..4833: skid_rings.items[1].segments[8].start
        zero(v.skid_rings[slot].items[1].segments[8].start);
        // 4834..4843: skid_rings.items[1].segments[8].end
        zero(v.skid_rings[slot].items[1].segments[8].end);
        // 4844..4853: skid_rings.items[1].segments[8].step
        zero(v.skid_rings[slot].items[1].segments[8].step);
        // 4854..4857: skid_rings.items[1].segments[8].first_vertex
        zero(v.skid_rings[slot].items[1].segments[8].first_vertex);
        // 4858..4867: skid_rings.items[1].segments[9].start
        zero(v.skid_rings[slot].items[1].segments[9].start);
        // 4868..4877: skid_rings.items[1].segments[9].end
        zero(v.skid_rings[slot].items[1].segments[9].end);
        // 4878..4887: skid_rings.items[1].segments[9].step
        zero(v.skid_rings[slot].items[1].segments[9].step);
        // 4888..488B: skid_rings.items[1].segments[9].first_vertex
        zero(v.skid_rings[slot].items[1].segments[9].first_vertex);
        // 488C..489B: skid_rings.items[1].segments[10].start
        zero(v.skid_rings[slot].items[1].segments[10].start);
        // 489C..48AB: skid_rings.items[1].segments[10].end
        zero(v.skid_rings[slot].items[1].segments[10].end);
        // 48AC..48BB: skid_rings.items[1].segments[10].step
        zero(v.skid_rings[slot].items[1].segments[10].step);
        // 48BC..48BF: skid_rings.items[1].segments[10].first_vertex
        zero(v.skid_rings[slot].items[1].segments[10].first_vertex);
        // 48C0..48CF: skid_rings.items[1].segments[11].start
        zero(v.skid_rings[slot].items[1].segments[11].start);
        // 48D0..48DF: skid_rings.items[1].segments[11].end
        zero(v.skid_rings[slot].items[1].segments[11].end);
        // 48E0..48EF: skid_rings.items[1].segments[11].step
        zero(v.skid_rings[slot].items[1].segments[11].step);
        // 48F0..48F3: skid_rings.items[1].segments[11].first_vertex
        zero(v.skid_rings[slot].items[1].segments[11].first_vertex);
        // 48F4..4903: skid_rings.items[1].segments[12].start
        zero(v.skid_rings[slot].items[1].segments[12].start);
        // 4904..4913: skid_rings.items[1].segments[12].end
        zero(v.skid_rings[slot].items[1].segments[12].end);
        // 4914..4923: skid_rings.items[1].segments[12].step
        zero(v.skid_rings[slot].items[1].segments[12].step);
        // 4924..4927: skid_rings.items[1].segments[12].first_vertex
        zero(v.skid_rings[slot].items[1].segments[12].first_vertex);
        // 4928..4937: skid_rings.items[1].segments[13].start
        zero(v.skid_rings[slot].items[1].segments[13].start);
        // 4938..4947: skid_rings.items[1].segments[13].end
        zero(v.skid_rings[slot].items[1].segments[13].end);
        // 4948..4957: skid_rings.items[1].segments[13].step
        zero(v.skid_rings[slot].items[1].segments[13].step);
        // 4958..495B: skid_rings.items[1].segments[13].first_vertex
        zero(v.skid_rings[slot].items[1].segments[13].first_vertex);
        // 495C..496B: skid_rings.items[1].segments[14].start
        zero(v.skid_rings[slot].items[1].segments[14].start);
        // 496C..497B: skid_rings.items[1].segments[14].end
        zero(v.skid_rings[slot].items[1].segments[14].end);
        // 497C..498B: skid_rings.items[1].segments[14].step
        zero(v.skid_rings[slot].items[1].segments[14].step);
        // 498C..498F: skid_rings.items[1].segments[14].first_vertex
        zero(v.skid_rings[slot].items[1].segments[14].first_vertex);
        // 4990..499F: skid_rings.items[1].segments[15].start
        zero(v.skid_rings[slot].items[1].segments[15].start);
        // 49A0..49AF: skid_rings.items[1].segments[15].end
        zero(v.skid_rings[slot].items[1].segments[15].end);
        // 49B0..49BF: skid_rings.items[1].segments[15].step
        zero(v.skid_rings[slot].items[1].segments[15].step);
        // 49C0..49C3: skid_rings.items[1].segments[15].first_vertex
        zero(v.skid_rings[slot].items[1].segments[15].first_vertex);
        // 49C4..49D3: skid_rings.items[1].segments[16].start
        zero(v.skid_rings[slot].items[1].segments[16].start);
        // 49D4..49E3: skid_rings.items[1].segments[16].end
        zero(v.skid_rings[slot].items[1].segments[16].end);
        // 49E4..49F3: skid_rings.items[1].segments[16].step
        zero(v.skid_rings[slot].items[1].segments[16].step);
        // 49F4..49F7: skid_rings.items[1].segments[16].first_vertex
        zero(v.skid_rings[slot].items[1].segments[16].first_vertex);
        // 49F8..4A07: skid_rings.items[1].segments[17].start
        zero(v.skid_rings[slot].items[1].segments[17].start);
        // 4A08..4A17: skid_rings.items[1].segments[17].end
        zero(v.skid_rings[slot].items[1].segments[17].end);
        // 4A18..4A27: skid_rings.items[1].segments[17].step
        zero(v.skid_rings[slot].items[1].segments[17].step);
        // 4A28..4A2B: skid_rings.items[1].segments[17].first_vertex
        zero(v.skid_rings[slot].items[1].segments[17].first_vertex);
        // 4A2C..4A3B: skid_rings.items[1].segments[18].start
        zero(v.skid_rings[slot].items[1].segments[18].start);
        // 4A3C..4A4B: skid_rings.items[1].segments[18].end
        zero(v.skid_rings[slot].items[1].segments[18].end);
        // 4A4C..4A5B: skid_rings.items[1].segments[18].step
        zero(v.skid_rings[slot].items[1].segments[18].step);
        // 4A5C..4A5F: skid_rings.items[1].segments[18].first_vertex
        zero(v.skid_rings[slot].items[1].segments[18].first_vertex);
        // 4A60..4A6F: skid_rings.items[1].segments[19].start
        zero(v.skid_rings[slot].items[1].segments[19].start);
        // 4A70..4A7F: skid_rings.items[1].segments[19].end
        zero(v.skid_rings[slot].items[1].segments[19].end);
        // 4A80..4A8F: skid_rings.items[1].segments[19].step
        zero(v.skid_rings[slot].items[1].segments[19].step);
        // 4A90..4A93: skid_rings.items[1].segments[19].first_vertex
        zero(v.skid_rings[slot].items[1].segments[19].first_vertex);
        // 4A94..4AA3: skid_rings.items[1].segments[20].start
        zero(v.skid_rings[slot].items[1].segments[20].start);
        // 4AA4..4AB3: skid_rings.items[1].segments[20].end
        zero(v.skid_rings[slot].items[1].segments[20].end);
        // 4AB4..4AC3: skid_rings.items[1].segments[20].step
        zero(v.skid_rings[slot].items[1].segments[20].step);
        // 4AC4..4AC7: skid_rings.items[1].segments[20].first_vertex
        zero(v.skid_rings[slot].items[1].segments[20].first_vertex);
        // 4AC8..4AD7: skid_rings.items[1].segments[21].start
        zero(v.skid_rings[slot].items[1].segments[21].start);
        // 4AD8..4AE7: skid_rings.items[1].segments[21].end
        zero(v.skid_rings[slot].items[1].segments[21].end);
        // 4AE8..4AF7: skid_rings.items[1].segments[21].step
        zero(v.skid_rings[slot].items[1].segments[21].step);
        // 4AF8..4AFB: skid_rings.items[1].segments[21].first_vertex
        zero(v.skid_rings[slot].items[1].segments[21].first_vertex);
        // 4AFC..4B0B: skid_rings.items[1].segments[22].start
        zero(v.skid_rings[slot].items[1].segments[22].start);
        // 4B0C..4B1B: skid_rings.items[1].segments[22].end
        zero(v.skid_rings[slot].items[1].segments[22].end);
        // 4B1C..4B2B: skid_rings.items[1].segments[22].step
        zero(v.skid_rings[slot].items[1].segments[22].step);
        // 4B2C..4B2F: skid_rings.items[1].segments[22].first_vertex
        zero(v.skid_rings[slot].items[1].segments[22].first_vertex);
        // 4B30..4B3F: skid_rings.items[1].segments[23].start
        zero(v.skid_rings[slot].items[1].segments[23].start);
        // 4B40..4B4F: skid_rings.items[1].segments[23].end
        zero(v.skid_rings[slot].items[1].segments[23].end);
        // 4B50..4B5F: skid_rings.items[1].segments[23].step
        zero(v.skid_rings[slot].items[1].segments[23].step);
        // 4B60..4B63: skid_rings.items[1].segments[23].first_vertex
        zero(v.skid_rings[slot].items[1].segments[23].first_vertex);
        // 4B64..4B73: skid_rings.items[1].segments[24].start
        zero(v.skid_rings[slot].items[1].segments[24].start);
        // 4B74..4B83: skid_rings.items[1].segments[24].end
        zero(v.skid_rings[slot].items[1].segments[24].end);
        // 4B84..4B93: skid_rings.items[1].segments[24].step
        zero(v.skid_rings[slot].items[1].segments[24].step);
        // 4B94..4B97: skid_rings.items[1].segments[24].first_vertex
        zero(v.skid_rings[slot].items[1].segments[24].first_vertex);
        // 4B98..4BA7: skid_rings.items[1].segments[25].start
        zero(v.skid_rings[slot].items[1].segments[25].start);
        // 4BA8..4BB7: skid_rings.items[1].segments[25].end
        zero(v.skid_rings[slot].items[1].segments[25].end);
        // 4BB8..4BC7: skid_rings.items[1].segments[25].step
        zero(v.skid_rings[slot].items[1].segments[25].step);
        // 4BC8..4BCB: skid_rings.items[1].segments[25].first_vertex
        zero(v.skid_rings[slot].items[1].segments[25].first_vertex);
        // 4BCC..4BDB: skid_rings.items[1].segments[26].start
        zero(v.skid_rings[slot].items[1].segments[26].start);
        // 4BDC..4BEB: skid_rings.items[1].segments[26].end
        zero(v.skid_rings[slot].items[1].segments[26].end);
        // 4BEC..4BFB: skid_rings.items[1].segments[26].step
        zero(v.skid_rings[slot].items[1].segments[26].step);
        // 4BFC..4BFF: skid_rings.items[1].segments[26].first_vertex
        zero(v.skid_rings[slot].items[1].segments[26].first_vertex);
        // 4C00..4C0F: skid_rings.items[1].segments[27].start
        zero(v.skid_rings[slot].items[1].segments[27].start);
        // 4C10..4C1F: skid_rings.items[1].segments[27].end
        zero(v.skid_rings[slot].items[1].segments[27].end);
        // 4C20..4C2F: skid_rings.items[1].segments[27].step
        zero(v.skid_rings[slot].items[1].segments[27].step);
        // 4C30..4C33: skid_rings.items[1].segments[27].first_vertex
        zero(v.skid_rings[slot].items[1].segments[27].first_vertex);
        // 4C34..4C43: skid_rings.items[1].segments[28].start
        zero(v.skid_rings[slot].items[1].segments[28].start);
        // 4C44..4C53: skid_rings.items[1].segments[28].end
        zero(v.skid_rings[slot].items[1].segments[28].end);
        // 4C54..4C63: skid_rings.items[1].segments[28].step
        zero(v.skid_rings[slot].items[1].segments[28].step);
        // 4C64..4C67: skid_rings.items[1].segments[28].first_vertex
        zero(v.skid_rings[slot].items[1].segments[28].first_vertex);
        // 4C68..4C77: skid_rings.items[1].segments[29].start
        zero(v.skid_rings[slot].items[1].segments[29].start);
        // 4C78..4C87: skid_rings.items[1].segments[29].end
        zero(v.skid_rings[slot].items[1].segments[29].end);
        // 4C88..4C97: skid_rings.items[1].segments[29].step
        zero(v.skid_rings[slot].items[1].segments[29].step);
        // 4C98..4C9B: skid_rings.items[1].segments[29].first_vertex
        zero(v.skid_rings[slot].items[1].segments[29].first_vertex);
        // 4C9C..4CAB: skid_rings.items[1].segments[30].start
        zero(v.skid_rings[slot].items[1].segments[30].start);
        // 4CAC..4CBB: skid_rings.items[1].segments[30].end
        zero(v.skid_rings[slot].items[1].segments[30].end);
        // 4CBC..4CCB: skid_rings.items[1].segments[30].step
        zero(v.skid_rings[slot].items[1].segments[30].step);
        // 4CCC..4CCF: skid_rings.items[1].segments[30].first_vertex
        zero(v.skid_rings[slot].items[1].segments[30].first_vertex);
        // 4CD0..4CDF: skid_rings.items[1].segments[31].start
        zero(v.skid_rings[slot].items[1].segments[31].start);
        // 4CE0..4CEF: skid_rings.items[1].segments[31].end
        zero(v.skid_rings[slot].items[1].segments[31].end);
        // 4CF0..4CFF: skid_rings.items[1].segments[31].step
        zero(v.skid_rings[slot].items[1].segments[31].step);
        // 4D00..4D03: skid_rings.items[1].segments[31].first_vertex
        zero(v.skid_rings[slot].items[1].segments[31].first_vertex);
        // 4D04..4D13: skid_rings.items[1].segments[32].start
        zero(v.skid_rings[slot].items[1].segments[32].start);
        // 4D14..4D23: skid_rings.items[1].segments[32].end
        zero(v.skid_rings[slot].items[1].segments[32].end);
        // 4D24..4D33: skid_rings.items[1].segments[32].step
        zero(v.skid_rings[slot].items[1].segments[32].step);
        // 4D34..4D37: skid_rings.items[1].segments[32].first_vertex
        zero(v.skid_rings[slot].items[1].segments[32].first_vertex);
        // 4D38..4D47: skid_rings.items[1].segments[33].start
        zero(v.skid_rings[slot].items[1].segments[33].start);
        // 4D48..4D57: skid_rings.items[1].segments[33].end
        zero(v.skid_rings[slot].items[1].segments[33].end);
        // 4D58..4D67: skid_rings.items[1].segments[33].step
        zero(v.skid_rings[slot].items[1].segments[33].step);
        // 4D68..4D6B: skid_rings.items[1].segments[33].first_vertex
        zero(v.skid_rings[slot].items[1].segments[33].first_vertex);
        // 4D6C..4D7B: skid_rings.items[1].segments[34].start
        zero(v.skid_rings[slot].items[1].segments[34].start);
        // 4D7C..4D8B: skid_rings.items[1].segments[34].end
        zero(v.skid_rings[slot].items[1].segments[34].end);
        // 4D8C..4D9B: skid_rings.items[1].segments[34].step
        zero(v.skid_rings[slot].items[1].segments[34].step);
        // 4D9C..4D9F: skid_rings.items[1].segments[34].first_vertex
        zero(v.skid_rings[slot].items[1].segments[34].first_vertex);
        // 4DA0..4DAF: skid_rings.items[1].segments[35].start
        zero(v.skid_rings[slot].items[1].segments[35].start);
        // 4DB0..4DBF: skid_rings.items[1].segments[35].end
        zero(v.skid_rings[slot].items[1].segments[35].end);
        // 4DC0..4DCF: skid_rings.items[1].segments[35].step
        zero(v.skid_rings[slot].items[1].segments[35].step);
        // 4DD0..4DD3: skid_rings.items[1].segments[35].first_vertex
        zero(v.skid_rings[slot].items[1].segments[35].first_vertex);
        // 4DD4..4DE3: skid_rings.items[1].segments[36].start
        zero(v.skid_rings[slot].items[1].segments[36].start);
        // 4DE4..4DF3: skid_rings.items[1].segments[36].end
        zero(v.skid_rings[slot].items[1].segments[36].end);
        // 4DF4..4E03: skid_rings.items[1].segments[36].step
        zero(v.skid_rings[slot].items[1].segments[36].step);
        // 4E04..4E07: skid_rings.items[1].segments[36].first_vertex
        zero(v.skid_rings[slot].items[1].segments[36].first_vertex);
        // 4E08..4E17: skid_rings.items[1].segments[37].start
        zero(v.skid_rings[slot].items[1].segments[37].start);
        // 4E18..4E27: skid_rings.items[1].segments[37].end
        zero(v.skid_rings[slot].items[1].segments[37].end);
        // 4E28..4E37: skid_rings.items[1].segments[37].step
        zero(v.skid_rings[slot].items[1].segments[37].step);
        // 4E38..4E3B: skid_rings.items[1].segments[37].first_vertex
        zero(v.skid_rings[slot].items[1].segments[37].first_vertex);
        // 4E3C..4E4B: skid_rings.items[1].segments[38].start
        zero(v.skid_rings[slot].items[1].segments[38].start);
        // 4E4C..4E5B: skid_rings.items[1].segments[38].end
        zero(v.skid_rings[slot].items[1].segments[38].end);
        // 4E5C..4E6B: skid_rings.items[1].segments[38].step
        zero(v.skid_rings[slot].items[1].segments[38].step);
        // 4E6C..4E6F: skid_rings.items[1].segments[38].first_vertex
        zero(v.skid_rings[slot].items[1].segments[38].first_vertex);
        // 4E70..4E7F: skid_rings.items[1].segments[39].start
        zero(v.skid_rings[slot].items[1].segments[39].start);
        // 4E80..4E8F: skid_rings.items[1].segments[39].end
        zero(v.skid_rings[slot].items[1].segments[39].end);
        // 4E90..4E9F: skid_rings.items[1].segments[39].step
        zero(v.skid_rings[slot].items[1].segments[39].step);
        // 4EA0..4EA3: skid_rings.items[1].segments[39].first_vertex
        zero(v.skid_rings[slot].items[1].segments[39].first_vertex);
        // 4EA4..4EB3: skid_rings.items[1].segments[40].start
        zero(v.skid_rings[slot].items[1].segments[40].start);
        // 4EB4..4EC3: skid_rings.items[1].segments[40].end
        zero(v.skid_rings[slot].items[1].segments[40].end);
        // 4EC4..4ED3: skid_rings.items[1].segments[40].step
        zero(v.skid_rings[slot].items[1].segments[40].step);
        // 4ED4..4ED7: skid_rings.items[1].segments[40].first_vertex
        zero(v.skid_rings[slot].items[1].segments[40].first_vertex);
        // 4ED8..4EE7: skid_rings.items[1].segments[41].start
        zero(v.skid_rings[slot].items[1].segments[41].start);
        // 4EE8..4EF7: skid_rings.items[1].segments[41].end
        zero(v.skid_rings[slot].items[1].segments[41].end);
        // 4EF8..4F07: skid_rings.items[1].segments[41].step
        zero(v.skid_rings[slot].items[1].segments[41].step);
        // 4F08..4F0B: skid_rings.items[1].segments[41].first_vertex
        zero(v.skid_rings[slot].items[1].segments[41].first_vertex);
        // 4F0C..4F1B: skid_rings.items[1].segments[42].start
        zero(v.skid_rings[slot].items[1].segments[42].start);
        // 4F1C..4F2B: skid_rings.items[1].segments[42].end
        zero(v.skid_rings[slot].items[1].segments[42].end);
        // 4F2C..4F3B: skid_rings.items[1].segments[42].step
        zero(v.skid_rings[slot].items[1].segments[42].step);
        // 4F3C..4F3F: skid_rings.items[1].segments[42].first_vertex
        zero(v.skid_rings[slot].items[1].segments[42].first_vertex);
        // 4F40..4F4F: skid_rings.items[1].segments[43].start
        zero(v.skid_rings[slot].items[1].segments[43].start);
        // 4F50..4F5F: skid_rings.items[1].segments[43].end
        zero(v.skid_rings[slot].items[1].segments[43].end);
        // 4F60..4F6F: skid_rings.items[1].segments[43].step
        zero(v.skid_rings[slot].items[1].segments[43].step);
        // 4F70..4F73: skid_rings.items[1].segments[43].first_vertex
        zero(v.skid_rings[slot].items[1].segments[43].first_vertex);
        // 4F74..4F83: skid_rings.items[1].segments[44].start
        zero(v.skid_rings[slot].items[1].segments[44].start);
        // 4F84..4F93: skid_rings.items[1].segments[44].end
        zero(v.skid_rings[slot].items[1].segments[44].end);
        // 4F94..4FA3: skid_rings.items[1].segments[44].step
        zero(v.skid_rings[slot].items[1].segments[44].step);
        // 4FA4..4FA7: skid_rings.items[1].segments[44].first_vertex
        zero(v.skid_rings[slot].items[1].segments[44].first_vertex);
        // 4FA8..4FB7: skid_rings.items[1].segments[45].start
        zero(v.skid_rings[slot].items[1].segments[45].start);
        // 4FB8..4FC7: skid_rings.items[1].segments[45].end
        zero(v.skid_rings[slot].items[1].segments[45].end);
        // 4FC8..4FD7: skid_rings.items[1].segments[45].step
        zero(v.skid_rings[slot].items[1].segments[45].step);
        // 4FD8..4FDB: skid_rings.items[1].segments[45].first_vertex
        zero(v.skid_rings[slot].items[1].segments[45].first_vertex);
        // 4FDC..4FEB: skid_rings.items[1].segments[46].start
        zero(v.skid_rings[slot].items[1].segments[46].start);
        // 4FEC..4FFB: skid_rings.items[1].segments[46].end
        zero(v.skid_rings[slot].items[1].segments[46].end);
        // 4FFC..500B: skid_rings.items[1].segments[46].step
        zero(v.skid_rings[slot].items[1].segments[46].step);
        // 500C..500F: skid_rings.items[1].segments[46].first_vertex
        zero(v.skid_rings[slot].items[1].segments[46].first_vertex);
        // 5010..501F: skid_rings.items[1].segments[47].start
        zero(v.skid_rings[slot].items[1].segments[47].start);
        // 5020..502F: skid_rings.items[1].segments[47].end
        zero(v.skid_rings[slot].items[1].segments[47].end);
        // 5030..503F: skid_rings.items[1].segments[47].step
        zero(v.skid_rings[slot].items[1].segments[47].step);
        // 5040..5043: skid_rings.items[1].segments[47].first_vertex
        zero(v.skid_rings[slot].items[1].segments[47].first_vertex);
        // 5044..5053: skid_rings.items[1].segments[48].start
        zero(v.skid_rings[slot].items[1].segments[48].start);
        // 5054..5063: skid_rings.items[1].segments[48].end
        zero(v.skid_rings[slot].items[1].segments[48].end);
        // 5064..5073: skid_rings.items[1].segments[48].step
        zero(v.skid_rings[slot].items[1].segments[48].step);
        // 5074..5077: skid_rings.items[1].segments[48].first_vertex
        zero(v.skid_rings[slot].items[1].segments[48].first_vertex);
        // 5078..5087: skid_rings.items[1].segments[49].start
        zero(v.skid_rings[slot].items[1].segments[49].start);
        // 5088..5097: skid_rings.items[1].segments[49].end
        zero(v.skid_rings[slot].items[1].segments[49].end);
        // 5098..50A7: skid_rings.items[1].segments[49].step
        zero(v.skid_rings[slot].items[1].segments[49].step);
        // 50A8..50AB: skid_rings.items[1].segments[49].first_vertex
        zero(v.skid_rings[slot].items[1].segments[49].first_vertex);
        // 50AC..50BB: skid_rings.items[1].segments[50].start
        zero(v.skid_rings[slot].items[1].segments[50].start);
        // 50BC..50CB: skid_rings.items[1].segments[50].end
        zero(v.skid_rings[slot].items[1].segments[50].end);
        // 50CC..50DB: skid_rings.items[1].segments[50].step
        zero(v.skid_rings[slot].items[1].segments[50].step);
        // 50DC..50DF: skid_rings.items[1].segments[50].first_vertex
        zero(v.skid_rings[slot].items[1].segments[50].first_vertex);
        // 50E0..50EF: skid_rings.items[1].segments[51].start
        zero(v.skid_rings[slot].items[1].segments[51].start);
        // 50F0..50FF: skid_rings.items[1].segments[51].end
        zero(v.skid_rings[slot].items[1].segments[51].end);
        // 5100..510F: skid_rings.items[1].segments[51].step
        zero(v.skid_rings[slot].items[1].segments[51].step);
        // 5110..5113: skid_rings.items[1].segments[51].first_vertex
        zero(v.skid_rings[slot].items[1].segments[51].first_vertex);
        // 5114..5123: skid_rings.items[1].segments[52].start
        zero(v.skid_rings[slot].items[1].segments[52].start);
        // 5124..5133: skid_rings.items[1].segments[52].end
        zero(v.skid_rings[slot].items[1].segments[52].end);
        // 5134..5143: skid_rings.items[1].segments[52].step
        zero(v.skid_rings[slot].items[1].segments[52].step);
        // 5144..5147: skid_rings.items[1].segments[52].first_vertex
        zero(v.skid_rings[slot].items[1].segments[52].first_vertex);
        // 5148..5157: skid_rings.items[1].segments[53].start
        zero(v.skid_rings[slot].items[1].segments[53].start);
        // 5158..5167: skid_rings.items[1].segments[53].end
        zero(v.skid_rings[slot].items[1].segments[53].end);
        // 5168..5177: skid_rings.items[1].segments[53].step
        zero(v.skid_rings[slot].items[1].segments[53].step);
        // 5178..517B: skid_rings.items[1].segments[53].first_vertex
        zero(v.skid_rings[slot].items[1].segments[53].first_vertex);
        // 517C..518B: skid_rings.items[1].segments[54].start
        zero(v.skid_rings[slot].items[1].segments[54].start);
        // 518C..519B: skid_rings.items[1].segments[54].end
        zero(v.skid_rings[slot].items[1].segments[54].end);
        // 519C..51AB: skid_rings.items[1].segments[54].step
        zero(v.skid_rings[slot].items[1].segments[54].step);
        // 51AC..51AF: skid_rings.items[1].segments[54].first_vertex
        zero(v.skid_rings[slot].items[1].segments[54].first_vertex);
        // 51B0..51BF: skid_rings.items[1].segments[55].start
        zero(v.skid_rings[slot].items[1].segments[55].start);
        // 51C0..51CF: skid_rings.items[1].segments[55].end
        zero(v.skid_rings[slot].items[1].segments[55].end);
        // 51D0..51DF: skid_rings.items[1].segments[55].step
        zero(v.skid_rings[slot].items[1].segments[55].step);
        // 51E0..51E3: skid_rings.items[1].segments[55].first_vertex
        zero(v.skid_rings[slot].items[1].segments[55].first_vertex);
        // 51E4..51F3: skid_rings.items[1].segments[56].start
        zero(v.skid_rings[slot].items[1].segments[56].start);
        // 51F4..5203: skid_rings.items[1].segments[56].end
        zero(v.skid_rings[slot].items[1].segments[56].end);
        // 5204..5213: skid_rings.items[1].segments[56].step
        zero(v.skid_rings[slot].items[1].segments[56].step);
        // 5214..5217: skid_rings.items[1].segments[56].first_vertex
        zero(v.skid_rings[slot].items[1].segments[56].first_vertex);
        // 5218..5227: skid_rings.items[1].segments[57].start
        zero(v.skid_rings[slot].items[1].segments[57].start);
        // 5228..5237: skid_rings.items[1].segments[57].end
        zero(v.skid_rings[slot].items[1].segments[57].end);
        // 5238..5247: skid_rings.items[1].segments[57].step
        zero(v.skid_rings[slot].items[1].segments[57].step);
        // 5248..524B: skid_rings.items[1].segments[57].first_vertex
        zero(v.skid_rings[slot].items[1].segments[57].first_vertex);
        // 524C..525B: skid_rings.items[1].segments[58].start
        zero(v.skid_rings[slot].items[1].segments[58].start);
        // 525C..526B: skid_rings.items[1].segments[58].end
        zero(v.skid_rings[slot].items[1].segments[58].end);
        // 526C..527B: skid_rings.items[1].segments[58].step
        zero(v.skid_rings[slot].items[1].segments[58].step);
        // 527C..527F: skid_rings.items[1].segments[58].first_vertex
        zero(v.skid_rings[slot].items[1].segments[58].first_vertex);
        // 5280..528F: skid_rings.items[1].segments[59].start
        zero(v.skid_rings[slot].items[1].segments[59].start);
        // 5290..529F: skid_rings.items[1].segments[59].end
        zero(v.skid_rings[slot].items[1].segments[59].end);
        // 52A0..52AF: skid_rings.items[1].segments[59].step
        zero(v.skid_rings[slot].items[1].segments[59].step);
        // 52B0..52B3: skid_rings.items[1].segments[59].first_vertex
        zero(v.skid_rings[slot].items[1].segments[59].first_vertex);
        // 52B4..52C3: skid_rings.items[1].segments[60].start
        zero(v.skid_rings[slot].items[1].segments[60].start);
        // 52C4..52D3: skid_rings.items[1].segments[60].end
        zero(v.skid_rings[slot].items[1].segments[60].end);
        // 52D4..52E3: skid_rings.items[1].segments[60].step
        zero(v.skid_rings[slot].items[1].segments[60].step);
        // 52E4..52E7: skid_rings.items[1].segments[60].first_vertex
        zero(v.skid_rings[slot].items[1].segments[60].first_vertex);
        // 52E8..52F7: skid_rings.items[1].segments[61].start
        zero(v.skid_rings[slot].items[1].segments[61].start);
        // 52F8..5307: skid_rings.items[1].segments[61].end
        zero(v.skid_rings[slot].items[1].segments[61].end);
        // 5308..5317: skid_rings.items[1].segments[61].step
        zero(v.skid_rings[slot].items[1].segments[61].step);
        // 5318..531B: skid_rings.items[1].segments[61].first_vertex
        zero(v.skid_rings[slot].items[1].segments[61].first_vertex);
        // 531C..532B: skid_rings.items[1].segments[62].start
        zero(v.skid_rings[slot].items[1].segments[62].start);
        // 532C..533B: skid_rings.items[1].segments[62].end
        zero(v.skid_rings[slot].items[1].segments[62].end);
        // 533C..534B: skid_rings.items[1].segments[62].step
        zero(v.skid_rings[slot].items[1].segments[62].step);
        // 534C..534F: skid_rings.items[1].segments[62].first_vertex
        zero(v.skid_rings[slot].items[1].segments[62].first_vertex);
        // 5350..535F: skid_rings.items[1].segments[63].start
        zero(v.skid_rings[slot].items[1].segments[63].start);
        // 5360..536F: skid_rings.items[1].segments[63].end
        zero(v.skid_rings[slot].items[1].segments[63].end);
        // 5370..537F: skid_rings.items[1].segments[63].step
        zero(v.skid_rings[slot].items[1].segments[63].step);
        // 5380..5383: skid_rings.items[1].segments[63].first_vertex
        zero(v.skid_rings[slot].items[1].segments[63].first_vertex);
        // 5384..5387: strips.items[2].field_00
        zero(v.strips[slot].items[2].field_00);
        // 5388..538B: strips.items[2].field_04
        zero(v.strips[slot].items[2].field_04);
        // 538C..538F: strips.items[2].field_08
        zero(v.strips[slot].items[2].field_08);
        // 5390..5393: skid_rings.items[2].last_corner
        zero(v.skid_rings[slot].items[2].last_corner);
        // 5394..53A3: skid_rings.items[2].segments[0].start
        zero(v.skid_rings[slot].items[2].segments[0].start);
        // 53A4..53B3: skid_rings.items[2].segments[0].end
        zero(v.skid_rings[slot].items[2].segments[0].end);
        // 53B4..53C3: skid_rings.items[2].segments[0].step
        zero(v.skid_rings[slot].items[2].segments[0].step);
        // 53C4..53C7: skid_rings.items[2].segments[0].first_vertex
        zero(v.skid_rings[slot].items[2].segments[0].first_vertex);
        // 53C8..53D7: skid_rings.items[2].segments[1].start
        zero(v.skid_rings[slot].items[2].segments[1].start);
        // 53D8..53E7: skid_rings.items[2].segments[1].end
        zero(v.skid_rings[slot].items[2].segments[1].end);
        // 53E8..53F7: skid_rings.items[2].segments[1].step
        zero(v.skid_rings[slot].items[2].segments[1].step);
        // 53F8..53FB: skid_rings.items[2].segments[1].first_vertex
        zero(v.skid_rings[slot].items[2].segments[1].first_vertex);
        // 53FC..540B: skid_rings.items[2].segments[2].start
        zero(v.skid_rings[slot].items[2].segments[2].start);
        // 540C..541B: skid_rings.items[2].segments[2].end
        zero(v.skid_rings[slot].items[2].segments[2].end);
        // 541C..542B: skid_rings.items[2].segments[2].step
        zero(v.skid_rings[slot].items[2].segments[2].step);
        // 542C..542F: skid_rings.items[2].segments[2].first_vertex
        zero(v.skid_rings[slot].items[2].segments[2].first_vertex);
        // 5430..543F: skid_rings.items[2].segments[3].start
        zero(v.skid_rings[slot].items[2].segments[3].start);
        // 5440..544F: skid_rings.items[2].segments[3].end
        zero(v.skid_rings[slot].items[2].segments[3].end);
        // 5450..545F: skid_rings.items[2].segments[3].step
        zero(v.skid_rings[slot].items[2].segments[3].step);
        // 5460..5463: skid_rings.items[2].segments[3].first_vertex
        zero(v.skid_rings[slot].items[2].segments[3].first_vertex);
        // 5464..5473: skid_rings.items[2].segments[4].start
        zero(v.skid_rings[slot].items[2].segments[4].start);
        // 5474..5483: skid_rings.items[2].segments[4].end
        zero(v.skid_rings[slot].items[2].segments[4].end);
        // 5484..5493: skid_rings.items[2].segments[4].step
        zero(v.skid_rings[slot].items[2].segments[4].step);
        // 5494..5497: skid_rings.items[2].segments[4].first_vertex
        zero(v.skid_rings[slot].items[2].segments[4].first_vertex);
        // 5498..54A7: skid_rings.items[2].segments[5].start
        zero(v.skid_rings[slot].items[2].segments[5].start);
        // 54A8..54B7: skid_rings.items[2].segments[5].end
        zero(v.skid_rings[slot].items[2].segments[5].end);
        // 54B8..54C7: skid_rings.items[2].segments[5].step
        zero(v.skid_rings[slot].items[2].segments[5].step);
        // 54C8..54CB: skid_rings.items[2].segments[5].first_vertex
        zero(v.skid_rings[slot].items[2].segments[5].first_vertex);
        // 54CC..54DB: skid_rings.items[2].segments[6].start
        zero(v.skid_rings[slot].items[2].segments[6].start);
        // 54DC..54EB: skid_rings.items[2].segments[6].end
        zero(v.skid_rings[slot].items[2].segments[6].end);
        // 54EC..54FB: skid_rings.items[2].segments[6].step
        zero(v.skid_rings[slot].items[2].segments[6].step);
        // 54FC..54FF: skid_rings.items[2].segments[6].first_vertex
        zero(v.skid_rings[slot].items[2].segments[6].first_vertex);
        // 5500..550F: skid_rings.items[2].segments[7].start
        zero(v.skid_rings[slot].items[2].segments[7].start);
        // 5510..551F: skid_rings.items[2].segments[7].end
        zero(v.skid_rings[slot].items[2].segments[7].end);
        // 5520..552F: skid_rings.items[2].segments[7].step
        zero(v.skid_rings[slot].items[2].segments[7].step);
        // 5530..5533: skid_rings.items[2].segments[7].first_vertex
        zero(v.skid_rings[slot].items[2].segments[7].first_vertex);
        // 5534..5543: skid_rings.items[2].segments[8].start
        zero(v.skid_rings[slot].items[2].segments[8].start);
        // 5544..5553: skid_rings.items[2].segments[8].end
        zero(v.skid_rings[slot].items[2].segments[8].end);
        // 5554..5563: skid_rings.items[2].segments[8].step
        zero(v.skid_rings[slot].items[2].segments[8].step);
        // 5564..5567: skid_rings.items[2].segments[8].first_vertex
        zero(v.skid_rings[slot].items[2].segments[8].first_vertex);
        // 5568..5577: skid_rings.items[2].segments[9].start
        zero(v.skid_rings[slot].items[2].segments[9].start);
        // 5578..5587: skid_rings.items[2].segments[9].end
        zero(v.skid_rings[slot].items[2].segments[9].end);
        // 5588..5597: skid_rings.items[2].segments[9].step
        zero(v.skid_rings[slot].items[2].segments[9].step);
        // 5598..559B: skid_rings.items[2].segments[9].first_vertex
        zero(v.skid_rings[slot].items[2].segments[9].first_vertex);
        // 559C..55AB: skid_rings.items[2].segments[10].start
        zero(v.skid_rings[slot].items[2].segments[10].start);
        // 55AC..55BB: skid_rings.items[2].segments[10].end
        zero(v.skid_rings[slot].items[2].segments[10].end);
        // 55BC..55CB: skid_rings.items[2].segments[10].step
        zero(v.skid_rings[slot].items[2].segments[10].step);
        // 55CC..55CF: skid_rings.items[2].segments[10].first_vertex
        zero(v.skid_rings[slot].items[2].segments[10].first_vertex);
        // 55D0..55DF: skid_rings.items[2].segments[11].start
        zero(v.skid_rings[slot].items[2].segments[11].start);
        // 55E0..55EF: skid_rings.items[2].segments[11].end
        zero(v.skid_rings[slot].items[2].segments[11].end);
        // 55F0..55FF: skid_rings.items[2].segments[11].step
        zero(v.skid_rings[slot].items[2].segments[11].step);
        // 5600..5603: skid_rings.items[2].segments[11].first_vertex
        zero(v.skid_rings[slot].items[2].segments[11].first_vertex);
        // 5604..5613: skid_rings.items[2].segments[12].start
        zero(v.skid_rings[slot].items[2].segments[12].start);
        // 5614..5623: skid_rings.items[2].segments[12].end
        zero(v.skid_rings[slot].items[2].segments[12].end);
        // 5624..5633: skid_rings.items[2].segments[12].step
        zero(v.skid_rings[slot].items[2].segments[12].step);
        // 5634..5637: skid_rings.items[2].segments[12].first_vertex
        zero(v.skid_rings[slot].items[2].segments[12].first_vertex);
        // 5638..5647: skid_rings.items[2].segments[13].start
        zero(v.skid_rings[slot].items[2].segments[13].start);
        // 5648..5657: skid_rings.items[2].segments[13].end
        zero(v.skid_rings[slot].items[2].segments[13].end);
        // 5658..5667: skid_rings.items[2].segments[13].step
        zero(v.skid_rings[slot].items[2].segments[13].step);
        // 5668..566B: skid_rings.items[2].segments[13].first_vertex
        zero(v.skid_rings[slot].items[2].segments[13].first_vertex);
        // 566C..567B: skid_rings.items[2].segments[14].start
        zero(v.skid_rings[slot].items[2].segments[14].start);
        // 567C..568B: skid_rings.items[2].segments[14].end
        zero(v.skid_rings[slot].items[2].segments[14].end);
        // 568C..569B: skid_rings.items[2].segments[14].step
        zero(v.skid_rings[slot].items[2].segments[14].step);
        // 569C..569F: skid_rings.items[2].segments[14].first_vertex
        zero(v.skid_rings[slot].items[2].segments[14].first_vertex);
        // 56A0..56AF: skid_rings.items[2].segments[15].start
        zero(v.skid_rings[slot].items[2].segments[15].start);
        // 56B0..56BF: skid_rings.items[2].segments[15].end
        zero(v.skid_rings[slot].items[2].segments[15].end);
        // 56C0..56CF: skid_rings.items[2].segments[15].step
        zero(v.skid_rings[slot].items[2].segments[15].step);
        // 56D0..56D3: skid_rings.items[2].segments[15].first_vertex
        zero(v.skid_rings[slot].items[2].segments[15].first_vertex);
        // 56D4..56E3: skid_rings.items[2].segments[16].start
        zero(v.skid_rings[slot].items[2].segments[16].start);
        // 56E4..56F3: skid_rings.items[2].segments[16].end
        zero(v.skid_rings[slot].items[2].segments[16].end);
        // 56F4..5703: skid_rings.items[2].segments[16].step
        zero(v.skid_rings[slot].items[2].segments[16].step);
        // 5704..5707: skid_rings.items[2].segments[16].first_vertex
        zero(v.skid_rings[slot].items[2].segments[16].first_vertex);
        // 5708..5717: skid_rings.items[2].segments[17].start
        zero(v.skid_rings[slot].items[2].segments[17].start);
        // 5718..5727: skid_rings.items[2].segments[17].end
        zero(v.skid_rings[slot].items[2].segments[17].end);
        // 5728..5737: skid_rings.items[2].segments[17].step
        zero(v.skid_rings[slot].items[2].segments[17].step);
        // 5738..573B: skid_rings.items[2].segments[17].first_vertex
        zero(v.skid_rings[slot].items[2].segments[17].first_vertex);
        // 573C..574B: skid_rings.items[2].segments[18].start
        zero(v.skid_rings[slot].items[2].segments[18].start);
        // 574C..575B: skid_rings.items[2].segments[18].end
        zero(v.skid_rings[slot].items[2].segments[18].end);
        // 575C..576B: skid_rings.items[2].segments[18].step
        zero(v.skid_rings[slot].items[2].segments[18].step);
        // 576C..576F: skid_rings.items[2].segments[18].first_vertex
        zero(v.skid_rings[slot].items[2].segments[18].first_vertex);
        // 5770..577F: skid_rings.items[2].segments[19].start
        zero(v.skid_rings[slot].items[2].segments[19].start);
        // 5780..578F: skid_rings.items[2].segments[19].end
        zero(v.skid_rings[slot].items[2].segments[19].end);
        // 5790..579F: skid_rings.items[2].segments[19].step
        zero(v.skid_rings[slot].items[2].segments[19].step);
        // 57A0..57A3: skid_rings.items[2].segments[19].first_vertex
        zero(v.skid_rings[slot].items[2].segments[19].first_vertex);
        // 57A4..57B3: skid_rings.items[2].segments[20].start
        zero(v.skid_rings[slot].items[2].segments[20].start);
        // 57B4..57C3: skid_rings.items[2].segments[20].end
        zero(v.skid_rings[slot].items[2].segments[20].end);
        // 57C4..57D3: skid_rings.items[2].segments[20].step
        zero(v.skid_rings[slot].items[2].segments[20].step);
        // 57D4..57D7: skid_rings.items[2].segments[20].first_vertex
        zero(v.skid_rings[slot].items[2].segments[20].first_vertex);
        // 57D8..57E7: skid_rings.items[2].segments[21].start
        zero(v.skid_rings[slot].items[2].segments[21].start);
        // 57E8..57F7: skid_rings.items[2].segments[21].end
        zero(v.skid_rings[slot].items[2].segments[21].end);
        // 57F8..5807: skid_rings.items[2].segments[21].step
        zero(v.skid_rings[slot].items[2].segments[21].step);
        // 5808..580B: skid_rings.items[2].segments[21].first_vertex
        zero(v.skid_rings[slot].items[2].segments[21].first_vertex);
        // 580C..581B: skid_rings.items[2].segments[22].start
        zero(v.skid_rings[slot].items[2].segments[22].start);
        // 581C..582B: skid_rings.items[2].segments[22].end
        zero(v.skid_rings[slot].items[2].segments[22].end);
        // 582C..583B: skid_rings.items[2].segments[22].step
        zero(v.skid_rings[slot].items[2].segments[22].step);
        // 583C..583F: skid_rings.items[2].segments[22].first_vertex
        zero(v.skid_rings[slot].items[2].segments[22].first_vertex);
        // 5840..584F: skid_rings.items[2].segments[23].start
        zero(v.skid_rings[slot].items[2].segments[23].start);
        // 5850..585F: skid_rings.items[2].segments[23].end
        zero(v.skid_rings[slot].items[2].segments[23].end);
        // 5860..586F: skid_rings.items[2].segments[23].step
        zero(v.skid_rings[slot].items[2].segments[23].step);
        // 5870..5873: skid_rings.items[2].segments[23].first_vertex
        zero(v.skid_rings[slot].items[2].segments[23].first_vertex);
        // 5874..5883: skid_rings.items[2].segments[24].start
        zero(v.skid_rings[slot].items[2].segments[24].start);
        // 5884..5893: skid_rings.items[2].segments[24].end
        zero(v.skid_rings[slot].items[2].segments[24].end);
        // 5894..58A3: skid_rings.items[2].segments[24].step
        zero(v.skid_rings[slot].items[2].segments[24].step);
        // 58A4..58A7: skid_rings.items[2].segments[24].first_vertex
        zero(v.skid_rings[slot].items[2].segments[24].first_vertex);
        // 58A8..58B7: skid_rings.items[2].segments[25].start
        zero(v.skid_rings[slot].items[2].segments[25].start);
        // 58B8..58C7: skid_rings.items[2].segments[25].end
        zero(v.skid_rings[slot].items[2].segments[25].end);
        // 58C8..58D7: skid_rings.items[2].segments[25].step
        zero(v.skid_rings[slot].items[2].segments[25].step);
        // 58D8..58DB: skid_rings.items[2].segments[25].first_vertex
        zero(v.skid_rings[slot].items[2].segments[25].first_vertex);
        // 58DC..58EB: skid_rings.items[2].segments[26].start
        zero(v.skid_rings[slot].items[2].segments[26].start);
        // 58EC..58FB: skid_rings.items[2].segments[26].end
        zero(v.skid_rings[slot].items[2].segments[26].end);
        // 58FC..590B: skid_rings.items[2].segments[26].step
        zero(v.skid_rings[slot].items[2].segments[26].step);
        // 590C..590F: skid_rings.items[2].segments[26].first_vertex
        zero(v.skid_rings[slot].items[2].segments[26].first_vertex);
        // 5910..591F: skid_rings.items[2].segments[27].start
        zero(v.skid_rings[slot].items[2].segments[27].start);
        // 5920..592F: skid_rings.items[2].segments[27].end
        zero(v.skid_rings[slot].items[2].segments[27].end);
        // 5930..593F: skid_rings.items[2].segments[27].step
        zero(v.skid_rings[slot].items[2].segments[27].step);
        // 5940..5943: skid_rings.items[2].segments[27].first_vertex
        zero(v.skid_rings[slot].items[2].segments[27].first_vertex);
        // 5944..5953: skid_rings.items[2].segments[28].start
        zero(v.skid_rings[slot].items[2].segments[28].start);
        // 5954..5963: skid_rings.items[2].segments[28].end
        zero(v.skid_rings[slot].items[2].segments[28].end);
        // 5964..5973: skid_rings.items[2].segments[28].step
        zero(v.skid_rings[slot].items[2].segments[28].step);
        // 5974..5977: skid_rings.items[2].segments[28].first_vertex
        zero(v.skid_rings[slot].items[2].segments[28].first_vertex);
        // 5978..5987: skid_rings.items[2].segments[29].start
        zero(v.skid_rings[slot].items[2].segments[29].start);
        // 5988..5997: skid_rings.items[2].segments[29].end
        zero(v.skid_rings[slot].items[2].segments[29].end);
        // 5998..59A7: skid_rings.items[2].segments[29].step
        zero(v.skid_rings[slot].items[2].segments[29].step);
        // 59A8..59AB: skid_rings.items[2].segments[29].first_vertex
        zero(v.skid_rings[slot].items[2].segments[29].first_vertex);
        // 59AC..59BB: skid_rings.items[2].segments[30].start
        zero(v.skid_rings[slot].items[2].segments[30].start);
        // 59BC..59CB: skid_rings.items[2].segments[30].end
        zero(v.skid_rings[slot].items[2].segments[30].end);
        // 59CC..59DB: skid_rings.items[2].segments[30].step
        zero(v.skid_rings[slot].items[2].segments[30].step);
        // 59DC..59DF: skid_rings.items[2].segments[30].first_vertex
        zero(v.skid_rings[slot].items[2].segments[30].first_vertex);
        // 59E0..59EF: skid_rings.items[2].segments[31].start
        zero(v.skid_rings[slot].items[2].segments[31].start);
        // 59F0..59FF: skid_rings.items[2].segments[31].end
        zero(v.skid_rings[slot].items[2].segments[31].end);
        // 5A00..5A0F: skid_rings.items[2].segments[31].step
        zero(v.skid_rings[slot].items[2].segments[31].step);
        // 5A10..5A13: skid_rings.items[2].segments[31].first_vertex
        zero(v.skid_rings[slot].items[2].segments[31].first_vertex);
        // 5A14..5A23: skid_rings.items[2].segments[32].start
        zero(v.skid_rings[slot].items[2].segments[32].start);
        // 5A24..5A33: skid_rings.items[2].segments[32].end
        zero(v.skid_rings[slot].items[2].segments[32].end);
        // 5A34..5A43: skid_rings.items[2].segments[32].step
        zero(v.skid_rings[slot].items[2].segments[32].step);
        // 5A44..5A47: skid_rings.items[2].segments[32].first_vertex
        zero(v.skid_rings[slot].items[2].segments[32].first_vertex);
        // 5A48..5A57: skid_rings.items[2].segments[33].start
        zero(v.skid_rings[slot].items[2].segments[33].start);
        // 5A58..5A67: skid_rings.items[2].segments[33].end
        zero(v.skid_rings[slot].items[2].segments[33].end);
        // 5A68..5A77: skid_rings.items[2].segments[33].step
        zero(v.skid_rings[slot].items[2].segments[33].step);
        // 5A78..5A7B: skid_rings.items[2].segments[33].first_vertex
        zero(v.skid_rings[slot].items[2].segments[33].first_vertex);
        // 5A7C..5A8B: skid_rings.items[2].segments[34].start
        zero(v.skid_rings[slot].items[2].segments[34].start);
        // 5A8C..5A9B: skid_rings.items[2].segments[34].end
        zero(v.skid_rings[slot].items[2].segments[34].end);
        // 5A9C..5AAB: skid_rings.items[2].segments[34].step
        zero(v.skid_rings[slot].items[2].segments[34].step);
        // 5AAC..5AAF: skid_rings.items[2].segments[34].first_vertex
        zero(v.skid_rings[slot].items[2].segments[34].first_vertex);
        // 5AB0..5ABF: skid_rings.items[2].segments[35].start
        zero(v.skid_rings[slot].items[2].segments[35].start);
        // 5AC0..5ACF: skid_rings.items[2].segments[35].end
        zero(v.skid_rings[slot].items[2].segments[35].end);
        // 5AD0..5ADF: skid_rings.items[2].segments[35].step
        zero(v.skid_rings[slot].items[2].segments[35].step);
        // 5AE0..5AE3: skid_rings.items[2].segments[35].first_vertex
        zero(v.skid_rings[slot].items[2].segments[35].first_vertex);
        // 5AE4..5AF3: skid_rings.items[2].segments[36].start
        zero(v.skid_rings[slot].items[2].segments[36].start);
        // 5AF4..5B03: skid_rings.items[2].segments[36].end
        zero(v.skid_rings[slot].items[2].segments[36].end);
        // 5B04..5B13: skid_rings.items[2].segments[36].step
        zero(v.skid_rings[slot].items[2].segments[36].step);
        // 5B14..5B17: skid_rings.items[2].segments[36].first_vertex
        zero(v.skid_rings[slot].items[2].segments[36].first_vertex);
        // 5B18..5B27: skid_rings.items[2].segments[37].start
        zero(v.skid_rings[slot].items[2].segments[37].start);
        // 5B28..5B37: skid_rings.items[2].segments[37].end
        zero(v.skid_rings[slot].items[2].segments[37].end);
        // 5B38..5B47: skid_rings.items[2].segments[37].step
        zero(v.skid_rings[slot].items[2].segments[37].step);
        // 5B48..5B4B: skid_rings.items[2].segments[37].first_vertex
        zero(v.skid_rings[slot].items[2].segments[37].first_vertex);
        // 5B4C..5B5B: skid_rings.items[2].segments[38].start
        zero(v.skid_rings[slot].items[2].segments[38].start);
        // 5B5C..5B6B: skid_rings.items[2].segments[38].end
        zero(v.skid_rings[slot].items[2].segments[38].end);
        // 5B6C..5B7B: skid_rings.items[2].segments[38].step
        zero(v.skid_rings[slot].items[2].segments[38].step);
        // 5B7C..5B7F: skid_rings.items[2].segments[38].first_vertex
        zero(v.skid_rings[slot].items[2].segments[38].first_vertex);
        // 5B80..5B8F: skid_rings.items[2].segments[39].start
        zero(v.skid_rings[slot].items[2].segments[39].start);
        // 5B90..5B9F: skid_rings.items[2].segments[39].end
        zero(v.skid_rings[slot].items[2].segments[39].end);
        // 5BA0..5BAF: skid_rings.items[2].segments[39].step
        zero(v.skid_rings[slot].items[2].segments[39].step);
        // 5BB0..5BB3: skid_rings.items[2].segments[39].first_vertex
        zero(v.skid_rings[slot].items[2].segments[39].first_vertex);
        // 5BB4..5BC3: skid_rings.items[2].segments[40].start
        zero(v.skid_rings[slot].items[2].segments[40].start);
        // 5BC4..5BD3: skid_rings.items[2].segments[40].end
        zero(v.skid_rings[slot].items[2].segments[40].end);
        // 5BD4..5BE3: skid_rings.items[2].segments[40].step
        zero(v.skid_rings[slot].items[2].segments[40].step);
        // 5BE4..5BE7: skid_rings.items[2].segments[40].first_vertex
        zero(v.skid_rings[slot].items[2].segments[40].first_vertex);
        // 5BE8..5BF7: skid_rings.items[2].segments[41].start
        zero(v.skid_rings[slot].items[2].segments[41].start);
        // 5BF8..5C07: skid_rings.items[2].segments[41].end
        zero(v.skid_rings[slot].items[2].segments[41].end);
        // 5C08..5C17: skid_rings.items[2].segments[41].step
        zero(v.skid_rings[slot].items[2].segments[41].step);
        // 5C18..5C1B: skid_rings.items[2].segments[41].first_vertex
        zero(v.skid_rings[slot].items[2].segments[41].first_vertex);
        // 5C1C..5C2B: skid_rings.items[2].segments[42].start
        zero(v.skid_rings[slot].items[2].segments[42].start);
        // 5C2C..5C3B: skid_rings.items[2].segments[42].end
        zero(v.skid_rings[slot].items[2].segments[42].end);
        // 5C3C..5C4B: skid_rings.items[2].segments[42].step
        zero(v.skid_rings[slot].items[2].segments[42].step);
        // 5C4C..5C4F: skid_rings.items[2].segments[42].first_vertex
        zero(v.skid_rings[slot].items[2].segments[42].first_vertex);
        // 5C50..5C5F: skid_rings.items[2].segments[43].start
        zero(v.skid_rings[slot].items[2].segments[43].start);
        // 5C60..5C6F: skid_rings.items[2].segments[43].end
        zero(v.skid_rings[slot].items[2].segments[43].end);
        // 5C70..5C7F: skid_rings.items[2].segments[43].step
        zero(v.skid_rings[slot].items[2].segments[43].step);
        // 5C80..5C83: skid_rings.items[2].segments[43].first_vertex
        zero(v.skid_rings[slot].items[2].segments[43].first_vertex);
        // 5C84..5C93: skid_rings.items[2].segments[44].start
        zero(v.skid_rings[slot].items[2].segments[44].start);
        // 5C94..5CA3: skid_rings.items[2].segments[44].end
        zero(v.skid_rings[slot].items[2].segments[44].end);
        // 5CA4..5CB3: skid_rings.items[2].segments[44].step
        zero(v.skid_rings[slot].items[2].segments[44].step);
        // 5CB4..5CB7: skid_rings.items[2].segments[44].first_vertex
        zero(v.skid_rings[slot].items[2].segments[44].first_vertex);
        // 5CB8..5CC7: skid_rings.items[2].segments[45].start
        zero(v.skid_rings[slot].items[2].segments[45].start);
        // 5CC8..5CD7: skid_rings.items[2].segments[45].end
        zero(v.skid_rings[slot].items[2].segments[45].end);
        // 5CD8..5CE7: skid_rings.items[2].segments[45].step
        zero(v.skid_rings[slot].items[2].segments[45].step);
        // 5CE8..5CEB: skid_rings.items[2].segments[45].first_vertex
        zero(v.skid_rings[slot].items[2].segments[45].first_vertex);
        // 5CEC..5CFB: skid_rings.items[2].segments[46].start
        zero(v.skid_rings[slot].items[2].segments[46].start);
        // 5CFC..5D0B: skid_rings.items[2].segments[46].end
        zero(v.skid_rings[slot].items[2].segments[46].end);
        // 5D0C..5D1B: skid_rings.items[2].segments[46].step
        zero(v.skid_rings[slot].items[2].segments[46].step);
        // 5D1C..5D1F: skid_rings.items[2].segments[46].first_vertex
        zero(v.skid_rings[slot].items[2].segments[46].first_vertex);
        // 5D20..5D2F: skid_rings.items[2].segments[47].start
        zero(v.skid_rings[slot].items[2].segments[47].start);
        // 5D30..5D3F: skid_rings.items[2].segments[47].end
        zero(v.skid_rings[slot].items[2].segments[47].end);
        // 5D40..5D4F: skid_rings.items[2].segments[47].step
        zero(v.skid_rings[slot].items[2].segments[47].step);
        // 5D50..5D53: skid_rings.items[2].segments[47].first_vertex
        zero(v.skid_rings[slot].items[2].segments[47].first_vertex);
        // 5D54..5D63: skid_rings.items[2].segments[48].start
        zero(v.skid_rings[slot].items[2].segments[48].start);
        // 5D64..5D73: skid_rings.items[2].segments[48].end
        zero(v.skid_rings[slot].items[2].segments[48].end);
        // 5D74..5D83: skid_rings.items[2].segments[48].step
        zero(v.skid_rings[slot].items[2].segments[48].step);
        // 5D84..5D87: skid_rings.items[2].segments[48].first_vertex
        zero(v.skid_rings[slot].items[2].segments[48].first_vertex);
        // 5D88..5D97: skid_rings.items[2].segments[49].start
        zero(v.skid_rings[slot].items[2].segments[49].start);
        // 5D98..5DA7: skid_rings.items[2].segments[49].end
        zero(v.skid_rings[slot].items[2].segments[49].end);
        // 5DA8..5DB7: skid_rings.items[2].segments[49].step
        zero(v.skid_rings[slot].items[2].segments[49].step);
        // 5DB8..5DBB: skid_rings.items[2].segments[49].first_vertex
        zero(v.skid_rings[slot].items[2].segments[49].first_vertex);
        // 5DBC..5DCB: skid_rings.items[2].segments[50].start
        zero(v.skid_rings[slot].items[2].segments[50].start);
        // 5DCC..5DDB: skid_rings.items[2].segments[50].end
        zero(v.skid_rings[slot].items[2].segments[50].end);
        // 5DDC..5DEB: skid_rings.items[2].segments[50].step
        zero(v.skid_rings[slot].items[2].segments[50].step);
        // 5DEC..5DEF: skid_rings.items[2].segments[50].first_vertex
        zero(v.skid_rings[slot].items[2].segments[50].first_vertex);
        // 5DF0..5DFF: skid_rings.items[2].segments[51].start
        zero(v.skid_rings[slot].items[2].segments[51].start);
        // 5E00..5E0F: skid_rings.items[2].segments[51].end
        zero(v.skid_rings[slot].items[2].segments[51].end);
        // 5E10..5E1F: skid_rings.items[2].segments[51].step
        zero(v.skid_rings[slot].items[2].segments[51].step);
        // 5E20..5E23: skid_rings.items[2].segments[51].first_vertex
        zero(v.skid_rings[slot].items[2].segments[51].first_vertex);
        // 5E24..5E33: skid_rings.items[2].segments[52].start
        zero(v.skid_rings[slot].items[2].segments[52].start);
        // 5E34..5E43: skid_rings.items[2].segments[52].end
        zero(v.skid_rings[slot].items[2].segments[52].end);
        // 5E44..5E53: skid_rings.items[2].segments[52].step
        zero(v.skid_rings[slot].items[2].segments[52].step);
        // 5E54..5E57: skid_rings.items[2].segments[52].first_vertex
        zero(v.skid_rings[slot].items[2].segments[52].first_vertex);
        // 5E58..5E67: skid_rings.items[2].segments[53].start
        zero(v.skid_rings[slot].items[2].segments[53].start);
        // 5E68..5E77: skid_rings.items[2].segments[53].end
        zero(v.skid_rings[slot].items[2].segments[53].end);
        // 5E78..5E87: skid_rings.items[2].segments[53].step
        zero(v.skid_rings[slot].items[2].segments[53].step);
        // 5E88..5E8B: skid_rings.items[2].segments[53].first_vertex
        zero(v.skid_rings[slot].items[2].segments[53].first_vertex);
        // 5E8C..5E9B: skid_rings.items[2].segments[54].start
        zero(v.skid_rings[slot].items[2].segments[54].start);
        // 5E9C..5EAB: skid_rings.items[2].segments[54].end
        zero(v.skid_rings[slot].items[2].segments[54].end);
        // 5EAC..5EBB: skid_rings.items[2].segments[54].step
        zero(v.skid_rings[slot].items[2].segments[54].step);
        // 5EBC..5EBF: skid_rings.items[2].segments[54].first_vertex
        zero(v.skid_rings[slot].items[2].segments[54].first_vertex);
        // 5EC0..5ECF: skid_rings.items[2].segments[55].start
        zero(v.skid_rings[slot].items[2].segments[55].start);
        // 5ED0..5EDF: skid_rings.items[2].segments[55].end
        zero(v.skid_rings[slot].items[2].segments[55].end);
        // 5EE0..5EEF: skid_rings.items[2].segments[55].step
        zero(v.skid_rings[slot].items[2].segments[55].step);
        // 5EF0..5EF3: skid_rings.items[2].segments[55].first_vertex
        zero(v.skid_rings[slot].items[2].segments[55].first_vertex);
        // 5EF4..5F03: skid_rings.items[2].segments[56].start
        zero(v.skid_rings[slot].items[2].segments[56].start);
        // 5F04..5F13: skid_rings.items[2].segments[56].end
        zero(v.skid_rings[slot].items[2].segments[56].end);
        // 5F14..5F23: skid_rings.items[2].segments[56].step
        zero(v.skid_rings[slot].items[2].segments[56].step);
        // 5F24..5F27: skid_rings.items[2].segments[56].first_vertex
        zero(v.skid_rings[slot].items[2].segments[56].first_vertex);
        // 5F28..5F37: skid_rings.items[2].segments[57].start
        zero(v.skid_rings[slot].items[2].segments[57].start);
        // 5F38..5F47: skid_rings.items[2].segments[57].end
        zero(v.skid_rings[slot].items[2].segments[57].end);
        // 5F48..5F57: skid_rings.items[2].segments[57].step
        zero(v.skid_rings[slot].items[2].segments[57].step);
        // 5F58..5F5B: skid_rings.items[2].segments[57].first_vertex
        zero(v.skid_rings[slot].items[2].segments[57].first_vertex);
        // 5F5C..5F6B: skid_rings.items[2].segments[58].start
        zero(v.skid_rings[slot].items[2].segments[58].start);
        // 5F6C..5F7B: skid_rings.items[2].segments[58].end
        zero(v.skid_rings[slot].items[2].segments[58].end);
        // 5F7C..5F8B: skid_rings.items[2].segments[58].step
        zero(v.skid_rings[slot].items[2].segments[58].step);
        // 5F8C..5F8F: skid_rings.items[2].segments[58].first_vertex
        zero(v.skid_rings[slot].items[2].segments[58].first_vertex);
        // 5F90..5F9F: skid_rings.items[2].segments[59].start
        zero(v.skid_rings[slot].items[2].segments[59].start);
        // 5FA0..5FAF: skid_rings.items[2].segments[59].end
        zero(v.skid_rings[slot].items[2].segments[59].end);
        // 5FB0..5FBF: skid_rings.items[2].segments[59].step
        zero(v.skid_rings[slot].items[2].segments[59].step);
        // 5FC0..5FC3: skid_rings.items[2].segments[59].first_vertex
        zero(v.skid_rings[slot].items[2].segments[59].first_vertex);
        // 5FC4..5FD3: skid_rings.items[2].segments[60].start
        zero(v.skid_rings[slot].items[2].segments[60].start);
        // 5FD4..5FE3: skid_rings.items[2].segments[60].end
        zero(v.skid_rings[slot].items[2].segments[60].end);
        // 5FE4..5FF3: skid_rings.items[2].segments[60].step
        zero(v.skid_rings[slot].items[2].segments[60].step);
        // 5FF4..5FF7: skid_rings.items[2].segments[60].first_vertex
        zero(v.skid_rings[slot].items[2].segments[60].first_vertex);
        // 5FF8..6007: skid_rings.items[2].segments[61].start
        zero(v.skid_rings[slot].items[2].segments[61].start);
        // 6008..6017: skid_rings.items[2].segments[61].end
        zero(v.skid_rings[slot].items[2].segments[61].end);
        // 6018..6027: skid_rings.items[2].segments[61].step
        zero(v.skid_rings[slot].items[2].segments[61].step);
        // 6028..602B: skid_rings.items[2].segments[61].first_vertex
        zero(v.skid_rings[slot].items[2].segments[61].first_vertex);
        // 602C..603B: skid_rings.items[2].segments[62].start
        zero(v.skid_rings[slot].items[2].segments[62].start);
        // 603C..604B: skid_rings.items[2].segments[62].end
        zero(v.skid_rings[slot].items[2].segments[62].end);
        // 604C..605B: skid_rings.items[2].segments[62].step
        zero(v.skid_rings[slot].items[2].segments[62].step);
        // 605C..605F: skid_rings.items[2].segments[62].first_vertex
        zero(v.skid_rings[slot].items[2].segments[62].first_vertex);
        // 6060..606F: skid_rings.items[2].segments[63].start
        zero(v.skid_rings[slot].items[2].segments[63].start);
        // 6070..607F: skid_rings.items[2].segments[63].end
        zero(v.skid_rings[slot].items[2].segments[63].end);
        // 6080..608F: skid_rings.items[2].segments[63].step
        zero(v.skid_rings[slot].items[2].segments[63].step);
        // 6090..6093: skid_rings.items[2].segments[63].first_vertex
        zero(v.skid_rings[slot].items[2].segments[63].first_vertex);
        // 6094..6097: strips.items[3].field_00
        zero(v.strips[slot].items[3].field_00);
        // 6098..609B: strips.items[3].field_04
        zero(v.strips[slot].items[3].field_04);
        // 609C..609F: strips.items[3].field_08
        zero(v.strips[slot].items[3].field_08);
        // 60A0..60A3: skid_rings.items[3].last_corner
        zero(v.skid_rings[slot].items[3].last_corner);
        // 60A4..60B3: skid_rings.items[3].segments[0].start
        zero(v.skid_rings[slot].items[3].segments[0].start);
        // 60B4..60C3: skid_rings.items[3].segments[0].end
        zero(v.skid_rings[slot].items[3].segments[0].end);
        // 60C4..60D3: skid_rings.items[3].segments[0].step
        zero(v.skid_rings[slot].items[3].segments[0].step);
        // 60D4..60D7: skid_rings.items[3].segments[0].first_vertex
        zero(v.skid_rings[slot].items[3].segments[0].first_vertex);
        // 60D8..60E7: skid_rings.items[3].segments[1].start
        zero(v.skid_rings[slot].items[3].segments[1].start);
        // 60E8..60F7: skid_rings.items[3].segments[1].end
        zero(v.skid_rings[slot].items[3].segments[1].end);
        // 60F8..6107: skid_rings.items[3].segments[1].step
        zero(v.skid_rings[slot].items[3].segments[1].step);
        // 6108..610B: skid_rings.items[3].segments[1].first_vertex
        zero(v.skid_rings[slot].items[3].segments[1].first_vertex);
        // 610C..611B: skid_rings.items[3].segments[2].start
        zero(v.skid_rings[slot].items[3].segments[2].start);
        // 611C..612B: skid_rings.items[3].segments[2].end
        zero(v.skid_rings[slot].items[3].segments[2].end);
        // 612C..613B: skid_rings.items[3].segments[2].step
        zero(v.skid_rings[slot].items[3].segments[2].step);
        // 613C..613F: skid_rings.items[3].segments[2].first_vertex
        zero(v.skid_rings[slot].items[3].segments[2].first_vertex);
        // 6140..614F: skid_rings.items[3].segments[3].start
        zero(v.skid_rings[slot].items[3].segments[3].start);
        // 6150..615F: skid_rings.items[3].segments[3].end
        zero(v.skid_rings[slot].items[3].segments[3].end);
        // 6160..616F: skid_rings.items[3].segments[3].step
        zero(v.skid_rings[slot].items[3].segments[3].step);
        // 6170..6173: skid_rings.items[3].segments[3].first_vertex
        zero(v.skid_rings[slot].items[3].segments[3].first_vertex);
        // 6174..6183: skid_rings.items[3].segments[4].start
        zero(v.skid_rings[slot].items[3].segments[4].start);
        // 6184..6193: skid_rings.items[3].segments[4].end
        zero(v.skid_rings[slot].items[3].segments[4].end);
        // 6194..61A3: skid_rings.items[3].segments[4].step
        zero(v.skid_rings[slot].items[3].segments[4].step);
        // 61A4..61A7: skid_rings.items[3].segments[4].first_vertex
        zero(v.skid_rings[slot].items[3].segments[4].first_vertex);
        // 61A8..61B7: skid_rings.items[3].segments[5].start
        zero(v.skid_rings[slot].items[3].segments[5].start);
        // 61B8..61C7: skid_rings.items[3].segments[5].end
        zero(v.skid_rings[slot].items[3].segments[5].end);
        // 61C8..61D7: skid_rings.items[3].segments[5].step
        zero(v.skid_rings[slot].items[3].segments[5].step);
        // 61D8..61DB: skid_rings.items[3].segments[5].first_vertex
        zero(v.skid_rings[slot].items[3].segments[5].first_vertex);
        // 61DC..61EB: skid_rings.items[3].segments[6].start
        zero(v.skid_rings[slot].items[3].segments[6].start);
        // 61EC..61FB: skid_rings.items[3].segments[6].end
        zero(v.skid_rings[slot].items[3].segments[6].end);
        // 61FC..620B: skid_rings.items[3].segments[6].step
        zero(v.skid_rings[slot].items[3].segments[6].step);
        // 620C..620F: skid_rings.items[3].segments[6].first_vertex
        zero(v.skid_rings[slot].items[3].segments[6].first_vertex);
        // 6210..621F: skid_rings.items[3].segments[7].start
        zero(v.skid_rings[slot].items[3].segments[7].start);
        // 6220..622F: skid_rings.items[3].segments[7].end
        zero(v.skid_rings[slot].items[3].segments[7].end);
        // 6230..623F: skid_rings.items[3].segments[7].step
        zero(v.skid_rings[slot].items[3].segments[7].step);
        // 6240..6243: skid_rings.items[3].segments[7].first_vertex
        zero(v.skid_rings[slot].items[3].segments[7].first_vertex);
        // 6244..6253: skid_rings.items[3].segments[8].start
        zero(v.skid_rings[slot].items[3].segments[8].start);
        // 6254..6263: skid_rings.items[3].segments[8].end
        zero(v.skid_rings[slot].items[3].segments[8].end);
        // 6264..6273: skid_rings.items[3].segments[8].step
        zero(v.skid_rings[slot].items[3].segments[8].step);
        // 6274..6277: skid_rings.items[3].segments[8].first_vertex
        zero(v.skid_rings[slot].items[3].segments[8].first_vertex);
        // 6278..6287: skid_rings.items[3].segments[9].start
        zero(v.skid_rings[slot].items[3].segments[9].start);
        // 6288..6297: skid_rings.items[3].segments[9].end
        zero(v.skid_rings[slot].items[3].segments[9].end);
        // 6298..62A7: skid_rings.items[3].segments[9].step
        zero(v.skid_rings[slot].items[3].segments[9].step);
        // 62A8..62AB: skid_rings.items[3].segments[9].first_vertex
        zero(v.skid_rings[slot].items[3].segments[9].first_vertex);
        // 62AC..62BB: skid_rings.items[3].segments[10].start
        zero(v.skid_rings[slot].items[3].segments[10].start);
        // 62BC..62CB: skid_rings.items[3].segments[10].end
        zero(v.skid_rings[slot].items[3].segments[10].end);
        // 62CC..62DB: skid_rings.items[3].segments[10].step
        zero(v.skid_rings[slot].items[3].segments[10].step);
        // 62DC..62DF: skid_rings.items[3].segments[10].first_vertex
        zero(v.skid_rings[slot].items[3].segments[10].first_vertex);
        // 62E0..62EF: skid_rings.items[3].segments[11].start
        zero(v.skid_rings[slot].items[3].segments[11].start);
        // 62F0..62FF: skid_rings.items[3].segments[11].end
        zero(v.skid_rings[slot].items[3].segments[11].end);
        // 6300..630F: skid_rings.items[3].segments[11].step
        zero(v.skid_rings[slot].items[3].segments[11].step);
        // 6310..6313: skid_rings.items[3].segments[11].first_vertex
        zero(v.skid_rings[slot].items[3].segments[11].first_vertex);
        // 6314..6323: skid_rings.items[3].segments[12].start
        zero(v.skid_rings[slot].items[3].segments[12].start);
        // 6324..6333: skid_rings.items[3].segments[12].end
        zero(v.skid_rings[slot].items[3].segments[12].end);
        // 6334..6343: skid_rings.items[3].segments[12].step
        zero(v.skid_rings[slot].items[3].segments[12].step);
        // 6344..6347: skid_rings.items[3].segments[12].first_vertex
        zero(v.skid_rings[slot].items[3].segments[12].first_vertex);
        // 6348..6357: skid_rings.items[3].segments[13].start
        zero(v.skid_rings[slot].items[3].segments[13].start);
        // 6358..6367: skid_rings.items[3].segments[13].end
        zero(v.skid_rings[slot].items[3].segments[13].end);
        // 6368..6377: skid_rings.items[3].segments[13].step
        zero(v.skid_rings[slot].items[3].segments[13].step);
        // 6378..637B: skid_rings.items[3].segments[13].first_vertex
        zero(v.skid_rings[slot].items[3].segments[13].first_vertex);
        // 637C..638B: skid_rings.items[3].segments[14].start
        zero(v.skid_rings[slot].items[3].segments[14].start);
        // 638C..639B: skid_rings.items[3].segments[14].end
        zero(v.skid_rings[slot].items[3].segments[14].end);
        // 639C..63AB: skid_rings.items[3].segments[14].step
        zero(v.skid_rings[slot].items[3].segments[14].step);
        // 63AC..63AF: skid_rings.items[3].segments[14].first_vertex
        zero(v.skid_rings[slot].items[3].segments[14].first_vertex);
        // 63B0..63BF: skid_rings.items[3].segments[15].start
        zero(v.skid_rings[slot].items[3].segments[15].start);
        // 63C0..63CF: skid_rings.items[3].segments[15].end
        zero(v.skid_rings[slot].items[3].segments[15].end);
        // 63D0..63DF: skid_rings.items[3].segments[15].step
        zero(v.skid_rings[slot].items[3].segments[15].step);
        // 63E0..63E3: skid_rings.items[3].segments[15].first_vertex
        zero(v.skid_rings[slot].items[3].segments[15].first_vertex);
        // 63E4..63F3: skid_rings.items[3].segments[16].start
        zero(v.skid_rings[slot].items[3].segments[16].start);
        // 63F4..6403: skid_rings.items[3].segments[16].end
        zero(v.skid_rings[slot].items[3].segments[16].end);
        // 6404..6413: skid_rings.items[3].segments[16].step
        zero(v.skid_rings[slot].items[3].segments[16].step);
        // 6414..6417: skid_rings.items[3].segments[16].first_vertex
        zero(v.skid_rings[slot].items[3].segments[16].first_vertex);
        // 6418..6427: skid_rings.items[3].segments[17].start
        zero(v.skid_rings[slot].items[3].segments[17].start);
        // 6428..6437: skid_rings.items[3].segments[17].end
        zero(v.skid_rings[slot].items[3].segments[17].end);
        // 6438..6447: skid_rings.items[3].segments[17].step
        zero(v.skid_rings[slot].items[3].segments[17].step);
        // 6448..644B: skid_rings.items[3].segments[17].first_vertex
        zero(v.skid_rings[slot].items[3].segments[17].first_vertex);
        // 644C..645B: skid_rings.items[3].segments[18].start
        zero(v.skid_rings[slot].items[3].segments[18].start);
        // 645C..646B: skid_rings.items[3].segments[18].end
        zero(v.skid_rings[slot].items[3].segments[18].end);
        // 646C..647B: skid_rings.items[3].segments[18].step
        zero(v.skid_rings[slot].items[3].segments[18].step);
        // 647C..647F: skid_rings.items[3].segments[18].first_vertex
        zero(v.skid_rings[slot].items[3].segments[18].first_vertex);
        // 6480..648F: skid_rings.items[3].segments[19].start
        zero(v.skid_rings[slot].items[3].segments[19].start);
        // 6490..649F: skid_rings.items[3].segments[19].end
        zero(v.skid_rings[slot].items[3].segments[19].end);
        // 64A0..64AF: skid_rings.items[3].segments[19].step
        zero(v.skid_rings[slot].items[3].segments[19].step);
        // 64B0..64B3: skid_rings.items[3].segments[19].first_vertex
        zero(v.skid_rings[slot].items[3].segments[19].first_vertex);
        // 64B4..64C3: skid_rings.items[3].segments[20].start
        zero(v.skid_rings[slot].items[3].segments[20].start);
        // 64C4..64D3: skid_rings.items[3].segments[20].end
        zero(v.skid_rings[slot].items[3].segments[20].end);
        // 64D4..64E3: skid_rings.items[3].segments[20].step
        zero(v.skid_rings[slot].items[3].segments[20].step);
        // 64E4..64E7: skid_rings.items[3].segments[20].first_vertex
        zero(v.skid_rings[slot].items[3].segments[20].first_vertex);
        // 64E8..64F7: skid_rings.items[3].segments[21].start
        zero(v.skid_rings[slot].items[3].segments[21].start);
        // 64F8..6507: skid_rings.items[3].segments[21].end
        zero(v.skid_rings[slot].items[3].segments[21].end);
        // 6508..6517: skid_rings.items[3].segments[21].step
        zero(v.skid_rings[slot].items[3].segments[21].step);
        // 6518..651B: skid_rings.items[3].segments[21].first_vertex
        zero(v.skid_rings[slot].items[3].segments[21].first_vertex);
        // 651C..652B: skid_rings.items[3].segments[22].start
        zero(v.skid_rings[slot].items[3].segments[22].start);
        // 652C..653B: skid_rings.items[3].segments[22].end
        zero(v.skid_rings[slot].items[3].segments[22].end);
        // 653C..654B: skid_rings.items[3].segments[22].step
        zero(v.skid_rings[slot].items[3].segments[22].step);
        // 654C..654F: skid_rings.items[3].segments[22].first_vertex
        zero(v.skid_rings[slot].items[3].segments[22].first_vertex);
        // 6550..655F: skid_rings.items[3].segments[23].start
        zero(v.skid_rings[slot].items[3].segments[23].start);
        // 6560..656F: skid_rings.items[3].segments[23].end
        zero(v.skid_rings[slot].items[3].segments[23].end);
        // 6570..657F: skid_rings.items[3].segments[23].step
        zero(v.skid_rings[slot].items[3].segments[23].step);
        // 6580..6583: skid_rings.items[3].segments[23].first_vertex
        zero(v.skid_rings[slot].items[3].segments[23].first_vertex);
        // 6584..6593: skid_rings.items[3].segments[24].start
        zero(v.skid_rings[slot].items[3].segments[24].start);
        // 6594..65A3: skid_rings.items[3].segments[24].end
        zero(v.skid_rings[slot].items[3].segments[24].end);
        // 65A4..65B3: skid_rings.items[3].segments[24].step
        zero(v.skid_rings[slot].items[3].segments[24].step);
        // 65B4..65B7: skid_rings.items[3].segments[24].first_vertex
        zero(v.skid_rings[slot].items[3].segments[24].first_vertex);
        // 65B8..65C7: skid_rings.items[3].segments[25].start
        zero(v.skid_rings[slot].items[3].segments[25].start);
        // 65C8..65D7: skid_rings.items[3].segments[25].end
        zero(v.skid_rings[slot].items[3].segments[25].end);
        // 65D8..65E7: skid_rings.items[3].segments[25].step
        zero(v.skid_rings[slot].items[3].segments[25].step);
        // 65E8..65EB: skid_rings.items[3].segments[25].first_vertex
        zero(v.skid_rings[slot].items[3].segments[25].first_vertex);
        // 65EC..65FB: skid_rings.items[3].segments[26].start
        zero(v.skid_rings[slot].items[3].segments[26].start);
        // 65FC..660B: skid_rings.items[3].segments[26].end
        zero(v.skid_rings[slot].items[3].segments[26].end);
        // 660C..661B: skid_rings.items[3].segments[26].step
        zero(v.skid_rings[slot].items[3].segments[26].step);
        // 661C..661F: skid_rings.items[3].segments[26].first_vertex
        zero(v.skid_rings[slot].items[3].segments[26].first_vertex);
        // 6620..662F: skid_rings.items[3].segments[27].start
        zero(v.skid_rings[slot].items[3].segments[27].start);
        // 6630..663F: skid_rings.items[3].segments[27].end
        zero(v.skid_rings[slot].items[3].segments[27].end);
        // 6640..664F: skid_rings.items[3].segments[27].step
        zero(v.skid_rings[slot].items[3].segments[27].step);
        // 6650..6653: skid_rings.items[3].segments[27].first_vertex
        zero(v.skid_rings[slot].items[3].segments[27].first_vertex);
        // 6654..6663: skid_rings.items[3].segments[28].start
        zero(v.skid_rings[slot].items[3].segments[28].start);
        // 6664..6673: skid_rings.items[3].segments[28].end
        zero(v.skid_rings[slot].items[3].segments[28].end);
        // 6674..6683: skid_rings.items[3].segments[28].step
        zero(v.skid_rings[slot].items[3].segments[28].step);
        // 6684..6687: skid_rings.items[3].segments[28].first_vertex
        zero(v.skid_rings[slot].items[3].segments[28].first_vertex);
        // 6688..6697: skid_rings.items[3].segments[29].start
        zero(v.skid_rings[slot].items[3].segments[29].start);
        // 6698..66A7: skid_rings.items[3].segments[29].end
        zero(v.skid_rings[slot].items[3].segments[29].end);
        // 66A8..66B7: skid_rings.items[3].segments[29].step
        zero(v.skid_rings[slot].items[3].segments[29].step);
        // 66B8..66BB: skid_rings.items[3].segments[29].first_vertex
        zero(v.skid_rings[slot].items[3].segments[29].first_vertex);
        // 66BC..66CB: skid_rings.items[3].segments[30].start
        zero(v.skid_rings[slot].items[3].segments[30].start);
        // 66CC..66DB: skid_rings.items[3].segments[30].end
        zero(v.skid_rings[slot].items[3].segments[30].end);
        // 66DC..66EB: skid_rings.items[3].segments[30].step
        zero(v.skid_rings[slot].items[3].segments[30].step);
        // 66EC..66EF: skid_rings.items[3].segments[30].first_vertex
        zero(v.skid_rings[slot].items[3].segments[30].first_vertex);
        // 66F0..66FF: skid_rings.items[3].segments[31].start
        zero(v.skid_rings[slot].items[3].segments[31].start);
        // 6700..670F: skid_rings.items[3].segments[31].end
        zero(v.skid_rings[slot].items[3].segments[31].end);
        // 6710..671F: skid_rings.items[3].segments[31].step
        zero(v.skid_rings[slot].items[3].segments[31].step);
        // 6720..6723: skid_rings.items[3].segments[31].first_vertex
        zero(v.skid_rings[slot].items[3].segments[31].first_vertex);
        // 6724..6733: skid_rings.items[3].segments[32].start
        zero(v.skid_rings[slot].items[3].segments[32].start);
        // 6734..6743: skid_rings.items[3].segments[32].end
        zero(v.skid_rings[slot].items[3].segments[32].end);
        // 6744..6753: skid_rings.items[3].segments[32].step
        zero(v.skid_rings[slot].items[3].segments[32].step);
        // 6754..6757: skid_rings.items[3].segments[32].first_vertex
        zero(v.skid_rings[slot].items[3].segments[32].first_vertex);
        // 6758..6767: skid_rings.items[3].segments[33].start
        zero(v.skid_rings[slot].items[3].segments[33].start);
        // 6768..6777: skid_rings.items[3].segments[33].end
        zero(v.skid_rings[slot].items[3].segments[33].end);
        // 6778..6787: skid_rings.items[3].segments[33].step
        zero(v.skid_rings[slot].items[3].segments[33].step);
        // 6788..678B: skid_rings.items[3].segments[33].first_vertex
        zero(v.skid_rings[slot].items[3].segments[33].first_vertex);
        // 678C..679B: skid_rings.items[3].segments[34].start
        zero(v.skid_rings[slot].items[3].segments[34].start);
        // 679C..67AB: skid_rings.items[3].segments[34].end
        zero(v.skid_rings[slot].items[3].segments[34].end);
        // 67AC..67BB: skid_rings.items[3].segments[34].step
        zero(v.skid_rings[slot].items[3].segments[34].step);
        // 67BC..67BF: skid_rings.items[3].segments[34].first_vertex
        zero(v.skid_rings[slot].items[3].segments[34].first_vertex);
        // 67C0..67CF: skid_rings.items[3].segments[35].start
        zero(v.skid_rings[slot].items[3].segments[35].start);
        // 67D0..67DF: skid_rings.items[3].segments[35].end
        zero(v.skid_rings[slot].items[3].segments[35].end);
        // 67E0..67EF: skid_rings.items[3].segments[35].step
        zero(v.skid_rings[slot].items[3].segments[35].step);
        // 67F0..67F3: skid_rings.items[3].segments[35].first_vertex
        zero(v.skid_rings[slot].items[3].segments[35].first_vertex);
        // 67F4..6803: skid_rings.items[3].segments[36].start
        zero(v.skid_rings[slot].items[3].segments[36].start);
        // 6804..6813: skid_rings.items[3].segments[36].end
        zero(v.skid_rings[slot].items[3].segments[36].end);
        // 6814..6823: skid_rings.items[3].segments[36].step
        zero(v.skid_rings[slot].items[3].segments[36].step);
        // 6824..6827: skid_rings.items[3].segments[36].first_vertex
        zero(v.skid_rings[slot].items[3].segments[36].first_vertex);
        // 6828..6837: skid_rings.items[3].segments[37].start
        zero(v.skid_rings[slot].items[3].segments[37].start);
        // 6838..6847: skid_rings.items[3].segments[37].end
        zero(v.skid_rings[slot].items[3].segments[37].end);
        // 6848..6857: skid_rings.items[3].segments[37].step
        zero(v.skid_rings[slot].items[3].segments[37].step);
        // 6858..685B: skid_rings.items[3].segments[37].first_vertex
        zero(v.skid_rings[slot].items[3].segments[37].first_vertex);
        // 685C..686B: skid_rings.items[3].segments[38].start
        zero(v.skid_rings[slot].items[3].segments[38].start);
        // 686C..687B: skid_rings.items[3].segments[38].end
        zero(v.skid_rings[slot].items[3].segments[38].end);
        // 687C..688B: skid_rings.items[3].segments[38].step
        zero(v.skid_rings[slot].items[3].segments[38].step);
        // 688C..688F: skid_rings.items[3].segments[38].first_vertex
        zero(v.skid_rings[slot].items[3].segments[38].first_vertex);
        // 6890..689F: skid_rings.items[3].segments[39].start
        zero(v.skid_rings[slot].items[3].segments[39].start);
        // 68A0..68AF: skid_rings.items[3].segments[39].end
        zero(v.skid_rings[slot].items[3].segments[39].end);
        // 68B0..68BF: skid_rings.items[3].segments[39].step
        zero(v.skid_rings[slot].items[3].segments[39].step);
        // 68C0..68C3: skid_rings.items[3].segments[39].first_vertex
        zero(v.skid_rings[slot].items[3].segments[39].first_vertex);
        // 68C4..68D3: skid_rings.items[3].segments[40].start
        zero(v.skid_rings[slot].items[3].segments[40].start);
        // 68D4..68E3: skid_rings.items[3].segments[40].end
        zero(v.skid_rings[slot].items[3].segments[40].end);
        // 68E4..68F3: skid_rings.items[3].segments[40].step
        zero(v.skid_rings[slot].items[3].segments[40].step);
        // 68F4..68F7: skid_rings.items[3].segments[40].first_vertex
        zero(v.skid_rings[slot].items[3].segments[40].first_vertex);
        // 68F8..6907: skid_rings.items[3].segments[41].start
        zero(v.skid_rings[slot].items[3].segments[41].start);
        // 6908..6917: skid_rings.items[3].segments[41].end
        zero(v.skid_rings[slot].items[3].segments[41].end);
        // 6918..6927: skid_rings.items[3].segments[41].step
        zero(v.skid_rings[slot].items[3].segments[41].step);
        // 6928..692B: skid_rings.items[3].segments[41].first_vertex
        zero(v.skid_rings[slot].items[3].segments[41].first_vertex);
        // 692C..693B: skid_rings.items[3].segments[42].start
        zero(v.skid_rings[slot].items[3].segments[42].start);
        // 693C..694B: skid_rings.items[3].segments[42].end
        zero(v.skid_rings[slot].items[3].segments[42].end);
        // 694C..695B: skid_rings.items[3].segments[42].step
        zero(v.skid_rings[slot].items[3].segments[42].step);
        // 695C..695F: skid_rings.items[3].segments[42].first_vertex
        zero(v.skid_rings[slot].items[3].segments[42].first_vertex);
        // 6960..696F: skid_rings.items[3].segments[43].start
        zero(v.skid_rings[slot].items[3].segments[43].start);
        // 6970..697F: skid_rings.items[3].segments[43].end
        zero(v.skid_rings[slot].items[3].segments[43].end);
        // 6980..698F: skid_rings.items[3].segments[43].step
        zero(v.skid_rings[slot].items[3].segments[43].step);
        // 6990..6993: skid_rings.items[3].segments[43].first_vertex
        zero(v.skid_rings[slot].items[3].segments[43].first_vertex);
        // 6994..69A3: skid_rings.items[3].segments[44].start
        zero(v.skid_rings[slot].items[3].segments[44].start);
        // 69A4..69B3: skid_rings.items[3].segments[44].end
        zero(v.skid_rings[slot].items[3].segments[44].end);
        // 69B4..69C3: skid_rings.items[3].segments[44].step
        zero(v.skid_rings[slot].items[3].segments[44].step);
        // 69C4..69C7: skid_rings.items[3].segments[44].first_vertex
        zero(v.skid_rings[slot].items[3].segments[44].first_vertex);
        // 69C8..69D7: skid_rings.items[3].segments[45].start
        zero(v.skid_rings[slot].items[3].segments[45].start);
        // 69D8..69E7: skid_rings.items[3].segments[45].end
        zero(v.skid_rings[slot].items[3].segments[45].end);
        // 69E8..69F7: skid_rings.items[3].segments[45].step
        zero(v.skid_rings[slot].items[3].segments[45].step);
        // 69F8..69FB: skid_rings.items[3].segments[45].first_vertex
        zero(v.skid_rings[slot].items[3].segments[45].first_vertex);
        // 69FC..6A0B: skid_rings.items[3].segments[46].start
        zero(v.skid_rings[slot].items[3].segments[46].start);
        // 6A0C..6A1B: skid_rings.items[3].segments[46].end
        zero(v.skid_rings[slot].items[3].segments[46].end);
        // 6A1C..6A2B: skid_rings.items[3].segments[46].step
        zero(v.skid_rings[slot].items[3].segments[46].step);
        // 6A2C..6A2F: skid_rings.items[3].segments[46].first_vertex
        zero(v.skid_rings[slot].items[3].segments[46].first_vertex);
        // 6A30..6A3F: skid_rings.items[3].segments[47].start
        zero(v.skid_rings[slot].items[3].segments[47].start);
        // 6A40..6A4F: skid_rings.items[3].segments[47].end
        zero(v.skid_rings[slot].items[3].segments[47].end);
        // 6A50..6A5F: skid_rings.items[3].segments[47].step
        zero(v.skid_rings[slot].items[3].segments[47].step);
        // 6A60..6A63: skid_rings.items[3].segments[47].first_vertex
        zero(v.skid_rings[slot].items[3].segments[47].first_vertex);
        // 6A64..6A73: skid_rings.items[3].segments[48].start
        zero(v.skid_rings[slot].items[3].segments[48].start);
        // 6A74..6A83: skid_rings.items[3].segments[48].end
        zero(v.skid_rings[slot].items[3].segments[48].end);
        // 6A84..6A93: skid_rings.items[3].segments[48].step
        zero(v.skid_rings[slot].items[3].segments[48].step);
        // 6A94..6A97: skid_rings.items[3].segments[48].first_vertex
        zero(v.skid_rings[slot].items[3].segments[48].first_vertex);
        // 6A98..6AA7: skid_rings.items[3].segments[49].start
        zero(v.skid_rings[slot].items[3].segments[49].start);
        // 6AA8..6AB7: skid_rings.items[3].segments[49].end
        zero(v.skid_rings[slot].items[3].segments[49].end);
        // 6AB8..6AC7: skid_rings.items[3].segments[49].step
        zero(v.skid_rings[slot].items[3].segments[49].step);
        // 6AC8..6ACB: skid_rings.items[3].segments[49].first_vertex
        zero(v.skid_rings[slot].items[3].segments[49].first_vertex);
        // 6ACC..6ADB: skid_rings.items[3].segments[50].start
        zero(v.skid_rings[slot].items[3].segments[50].start);
        // 6ADC..6AEB: skid_rings.items[3].segments[50].end
        zero(v.skid_rings[slot].items[3].segments[50].end);
        // 6AEC..6AFB: skid_rings.items[3].segments[50].step
        zero(v.skid_rings[slot].items[3].segments[50].step);
        // 6AFC..6AFF: skid_rings.items[3].segments[50].first_vertex
        zero(v.skid_rings[slot].items[3].segments[50].first_vertex);
        // 6B00..6B0F: skid_rings.items[3].segments[51].start
        zero(v.skid_rings[slot].items[3].segments[51].start);
        // 6B10..6B1F: skid_rings.items[3].segments[51].end
        zero(v.skid_rings[slot].items[3].segments[51].end);
        // 6B20..6B2F: skid_rings.items[3].segments[51].step
        zero(v.skid_rings[slot].items[3].segments[51].step);
        // 6B30..6B33: skid_rings.items[3].segments[51].first_vertex
        zero(v.skid_rings[slot].items[3].segments[51].first_vertex);
        // 6B34..6B43: skid_rings.items[3].segments[52].start
        zero(v.skid_rings[slot].items[3].segments[52].start);
        // 6B44..6B53: skid_rings.items[3].segments[52].end
        zero(v.skid_rings[slot].items[3].segments[52].end);
        // 6B54..6B63: skid_rings.items[3].segments[52].step
        zero(v.skid_rings[slot].items[3].segments[52].step);
        // 6B64..6B67: skid_rings.items[3].segments[52].first_vertex
        zero(v.skid_rings[slot].items[3].segments[52].first_vertex);
        // 6B68..6B77: skid_rings.items[3].segments[53].start
        zero(v.skid_rings[slot].items[3].segments[53].start);
        // 6B78..6B87: skid_rings.items[3].segments[53].end
        zero(v.skid_rings[slot].items[3].segments[53].end);
        // 6B88..6B97: skid_rings.items[3].segments[53].step
        zero(v.skid_rings[slot].items[3].segments[53].step);
        // 6B98..6B9B: skid_rings.items[3].segments[53].first_vertex
        zero(v.skid_rings[slot].items[3].segments[53].first_vertex);
        // 6B9C..6BAB: skid_rings.items[3].segments[54].start
        zero(v.skid_rings[slot].items[3].segments[54].start);
        // 6BAC..6BBB: skid_rings.items[3].segments[54].end
        zero(v.skid_rings[slot].items[3].segments[54].end);
        // 6BBC..6BCB: skid_rings.items[3].segments[54].step
        zero(v.skid_rings[slot].items[3].segments[54].step);
        // 6BCC..6BCF: skid_rings.items[3].segments[54].first_vertex
        zero(v.skid_rings[slot].items[3].segments[54].first_vertex);
        // 6BD0..6BDF: skid_rings.items[3].segments[55].start
        zero(v.skid_rings[slot].items[3].segments[55].start);
        // 6BE0..6BEF: skid_rings.items[3].segments[55].end
        zero(v.skid_rings[slot].items[3].segments[55].end);
        // 6BF0..6BFF: skid_rings.items[3].segments[55].step
        zero(v.skid_rings[slot].items[3].segments[55].step);
        // 6C00..6C03: skid_rings.items[3].segments[55].first_vertex
        zero(v.skid_rings[slot].items[3].segments[55].first_vertex);
        // 6C04..6C13: skid_rings.items[3].segments[56].start
        zero(v.skid_rings[slot].items[3].segments[56].start);
        // 6C14..6C23: skid_rings.items[3].segments[56].end
        zero(v.skid_rings[slot].items[3].segments[56].end);
        // 6C24..6C33: skid_rings.items[3].segments[56].step
        zero(v.skid_rings[slot].items[3].segments[56].step);
        // 6C34..6C37: skid_rings.items[3].segments[56].first_vertex
        zero(v.skid_rings[slot].items[3].segments[56].first_vertex);
        // 6C38..6C47: skid_rings.items[3].segments[57].start
        zero(v.skid_rings[slot].items[3].segments[57].start);
        // 6C48..6C57: skid_rings.items[3].segments[57].end
        zero(v.skid_rings[slot].items[3].segments[57].end);
        // 6C58..6C67: skid_rings.items[3].segments[57].step
        zero(v.skid_rings[slot].items[3].segments[57].step);
        // 6C68..6C6B: skid_rings.items[3].segments[57].first_vertex
        zero(v.skid_rings[slot].items[3].segments[57].first_vertex);
        // 6C6C..6C7B: skid_rings.items[3].segments[58].start
        zero(v.skid_rings[slot].items[3].segments[58].start);
        // 6C7C..6C8B: skid_rings.items[3].segments[58].end
        zero(v.skid_rings[slot].items[3].segments[58].end);
        // 6C8C..6C9B: skid_rings.items[3].segments[58].step
        zero(v.skid_rings[slot].items[3].segments[58].step);
        // 6C9C..6C9F: skid_rings.items[3].segments[58].first_vertex
        zero(v.skid_rings[slot].items[3].segments[58].first_vertex);
        // 6CA0..6CAF: skid_rings.items[3].segments[59].start
        zero(v.skid_rings[slot].items[3].segments[59].start);
        // 6CB0..6CBF: skid_rings.items[3].segments[59].end
        zero(v.skid_rings[slot].items[3].segments[59].end);
        // 6CC0..6CCF: skid_rings.items[3].segments[59].step
        zero(v.skid_rings[slot].items[3].segments[59].step);
        // 6CD0..6CD3: skid_rings.items[3].segments[59].first_vertex
        zero(v.skid_rings[slot].items[3].segments[59].first_vertex);
        // 6CD4..6CE3: skid_rings.items[3].segments[60].start
        zero(v.skid_rings[slot].items[3].segments[60].start);
        // 6CE4..6CF3: skid_rings.items[3].segments[60].end
        zero(v.skid_rings[slot].items[3].segments[60].end);
        // 6CF4..6D03: skid_rings.items[3].segments[60].step
        zero(v.skid_rings[slot].items[3].segments[60].step);
        // 6D04..6D07: skid_rings.items[3].segments[60].first_vertex
        zero(v.skid_rings[slot].items[3].segments[60].first_vertex);
        // 6D08..6D17: skid_rings.items[3].segments[61].start
        zero(v.skid_rings[slot].items[3].segments[61].start);
        // 6D18..6D27: skid_rings.items[3].segments[61].end
        zero(v.skid_rings[slot].items[3].segments[61].end);
        // 6D28..6D37: skid_rings.items[3].segments[61].step
        zero(v.skid_rings[slot].items[3].segments[61].step);
        // 6D38..6D3B: skid_rings.items[3].segments[61].first_vertex
        zero(v.skid_rings[slot].items[3].segments[61].first_vertex);
        // 6D3C..6D4B: skid_rings.items[3].segments[62].start
        zero(v.skid_rings[slot].items[3].segments[62].start);
        // 6D4C..6D5B: skid_rings.items[3].segments[62].end
        zero(v.skid_rings[slot].items[3].segments[62].end);
        // 6D5C..6D6B: skid_rings.items[3].segments[62].step
        zero(v.skid_rings[slot].items[3].segments[62].step);
        // 6D6C..6D6F: skid_rings.items[3].segments[62].first_vertex
        zero(v.skid_rings[slot].items[3].segments[62].first_vertex);
        // 6D70..6D7F: skid_rings.items[3].segments[63].start
        zero(v.skid_rings[slot].items[3].segments[63].start);
        // 6D80..6D8F: skid_rings.items[3].segments[63].end
        zero(v.skid_rings[slot].items[3].segments[63].end);
        // 6D90..6D9F: skid_rings.items[3].segments[63].step
        zero(v.skid_rings[slot].items[3].segments[63].step);
        // 6DA0..6DA3: skid_rings.items[3].segments[63].first_vertex
        zero(v.skid_rings[slot].items[3].segments[63].first_vertex);
        // 6DA4..6DB3: physics.stunt.phase_indices
        zero(v.physics[slot].stunt.phase_indices);
        // 6DB4..71C3: physics.stunt.history_and_balance
        zero(v.physics[slot].stunt.history_and_balance);
        // 71C4..71C7: physics.stunt.flip_phase
        zero(v.physics[slot].stunt.flip_phase);
        // 71C8..71CB: physics.stunt.flip_timer
        zero(v.physics[slot].stunt.flip_timer);
        // 71CC..71DB: physics.stunt.flip_up
        zero(v.physics[slot].stunt.flip_up);
        // 71DC..71DF: physics.stunt.drift_ticks
        zero(v.physics[slot].stunt.drift_ticks);
        // 71E0..71E3: physics.stunt.slipstream_ticks
        zero(v.physics[slot].stunt.slipstream_ticks);
        // 71E4..71E7: physics.stunt.roof_ticks
        zero(v.physics[slot].stunt.roof_ticks);
        // 71E8..71EB: physics.stunt.inversion_timer
        zero(v.physics[slot].stunt.inversion_timer);
        // 71EC..71EF: physics.contact_counter
        zero(v.physics[slot].contact_counter);
        // 71F0..71F3: physics.stunt.active
        zero(v.physics[slot].stunt.active);
        // 71F4..71F7: physics.stunt.previous_active
        zero(v.physics[slot].stunt.previous_active);
        // 71F8..71FB: physics.stunt.previous_contact_counter
        zero(v.physics[slot].stunt.previous_contact_counter);
        // 71FC..71FF: physics.airborne_ticks
        zero(v.physics[slot].airborne_ticks);
        // 7200..7203: air.completed_airtime
        zero(v.air[slot].completed_airtime);
        // 7204..7213: physics.stunt.accumulated_angles
        zero(v.physics[slot].stunt.accumulated_angles);
        // 7214..7223: physics.stunt.angular_velocity
        zero(v.physics[slot].stunt.angular_velocity);
        // 7224..7233: physics.stunt.angular_acceleration
        zero(v.physics[slot].stunt.angular_acceleration);
        // 7234..7237: physics.stunt.launch_tick
        zero(v.physics[slot].stunt.launch_tick);
        // 7238..723B: physics.stunt.launch_height
        zero(v.physics[slot].stunt.launch_height);
        // 723C..723F: physics.stunt.maximum_height
        zero(v.physics[slot].stunt.maximum_height);
        // 7240..7243: physics.stunt.height_level
        zero(v.physics[slot].stunt.height_level);
        // 7244..7247: physics.stunt.launch_state
        zero(v.physics[slot].stunt.launch_state);
        // 7248..724B: physics.stunt.landing_pending
        zero(v.physics[slot].stunt.landing_pending);
        // 724C..724F: physics.stunt.landing_counter
        zero(v.physics[slot].stunt.landing_counter);
        // 7250..7253: physics.stunt.landing_hold
        zero(v.physics[slot].stunt.landing_hold);
        // 7254..7257: physics.stunt.score_hold
        zero(v.physics[slot].stunt.score_hold);
        // 7258..725B: physics.stunt.chain_count
        zero(v.physics[slot].stunt.chain_count);
        // 725C..725F: physics.stunt.displayed_chain_count
        zero(v.physics[slot].stunt.displayed_chain_count);
        // 7260..7263: physics.stunt.chain_decay_active
        zero(v.physics[slot].stunt.chain_decay_active);
        // 7264..7267: physics.stunt.chain_decay_timer
        zero(v.physics[slot].stunt.chain_decay_timer);
        // 7268..726B: race.recording
        zero(v.race[slot].recording);
        // 726C..726F: contact.surface.object_mode
        zero(v.contact[slot].surface.object_mode);
        // 7270..727F: contact.body.position
        zero(v.contact[slot].body.position);
        // 7280..728F: contact.body.previous_position
        zero(v.contact[slot].body.previous_position);
        // 7290..729F: source_uninterpreted.offset_7290
        zero(v.source_uninterpreted[slot].offset_7290);
        // 72A0..72AF: contact.body.velocity
        zero(v.contact[slot].body.velocity);
        // 72B0..72B3: contact.body.mass
        zero(v.contact[slot].body.mass);
        // 72B4..72B7: contact.rig.bounding_radius
        zero(v.contact[slot].rig.bounding_radius);
        // 72B8..72F7: contact.body.orientation
        zero(v.contact[slot].body.orientation);
        // 72F8..7337: contact.body.incremental
        zero(v.contact[slot].body.incremental);
        // 7338..7377: contact.matrix_7338
        zero(v.contact[slot].matrix_7338);
        // 7378..737B: source_uninterpreted.offset_7378
        zero(v.source_uninterpreted[slot].offset_7378);
        // 737C..73AB: contact.orientation_snapshot[0..11]
        // Only 12 source lanes; host matrix lanes 12..15 are retained.
        for(unsigned lane=0;lane!=12;++lane)zero(v.contact[slot].orientation_snapshot[lane]);
        // 73AC..73AF: contact.rig.count
        zero(v.contact[slot].rig.count);
        // 73B0..73B3: contact.rig.origin_y_offset
        zero(v.contact[slot].rig.origin_y_offset);
        // 73B4..73C3: contact.rig.probes[0].local
        zero(v.contact[slot].rig.probes[0].local);
        // 73C4..73D3: contact.rig.probes[0].current
        zero(v.contact[slot].rig.probes[0].current);
        // 73D4..73E3: contact.rig.probes[0].previous
        zero(v.contact[slot].rig.probes[0].previous);
        // 73E4..73F3: contact.rig.probes[0].velocity
        zero(v.contact[slot].rig.probes[0].velocity);
        // 73F4..73F7: contact.rig.probes[0].contact_iterations
        zero(v.contact[slot].rig.probes[0].contact_iterations);
        // 73F8..73FB: contact.rig.probes[0].mass_fraction
        zero(v.contact[slot].rig.probes[0].mass_fraction);
        // 73FC..73FF: contact.rig.probes[0].radius
        zero(v.contact[slot].rig.probes[0].radius);
        // 7400..740F: contact.rig.probes[1].local
        zero(v.contact[slot].rig.probes[1].local);
        // 7410..741F: contact.rig.probes[1].current
        zero(v.contact[slot].rig.probes[1].current);
        // 7420..742F: contact.rig.probes[1].previous
        zero(v.contact[slot].rig.probes[1].previous);
        // 7430..743F: contact.rig.probes[1].velocity
        zero(v.contact[slot].rig.probes[1].velocity);
        // 7440..7443: contact.rig.probes[1].contact_iterations
        zero(v.contact[slot].rig.probes[1].contact_iterations);
        // 7444..7447: contact.rig.probes[1].mass_fraction
        zero(v.contact[slot].rig.probes[1].mass_fraction);
        // 7448..744B: contact.rig.probes[1].radius
        zero(v.contact[slot].rig.probes[1].radius);
        // 744C..745B: contact.rig.probes[2].local
        zero(v.contact[slot].rig.probes[2].local);
        // 745C..746B: contact.rig.probes[2].current
        zero(v.contact[slot].rig.probes[2].current);
        // 746C..747B: contact.rig.probes[2].previous
        zero(v.contact[slot].rig.probes[2].previous);
        // 747C..748B: contact.rig.probes[2].velocity
        zero(v.contact[slot].rig.probes[2].velocity);
        // 748C..748F: contact.rig.probes[2].contact_iterations
        zero(v.contact[slot].rig.probes[2].contact_iterations);
        // 7490..7493: contact.rig.probes[2].mass_fraction
        zero(v.contact[slot].rig.probes[2].mass_fraction);
        // 7494..7497: contact.rig.probes[2].radius
        zero(v.contact[slot].rig.probes[2].radius);
        // 7498..74A7: contact.rig.probes[3].local
        zero(v.contact[slot].rig.probes[3].local);
        // 74A8..74B7: contact.rig.probes[3].current
        zero(v.contact[slot].rig.probes[3].current);
        // 74B8..74C7: contact.rig.probes[3].previous
        zero(v.contact[slot].rig.probes[3].previous);
        // 74C8..74D7: contact.rig.probes[3].velocity
        zero(v.contact[slot].rig.probes[3].velocity);
        // 74D8..74DB: contact.rig.probes[3].contact_iterations
        zero(v.contact[slot].rig.probes[3].contact_iterations);
        // 74DC..74DF: contact.rig.probes[3].mass_fraction
        zero(v.contact[slot].rig.probes[3].mass_fraction);
        // 74E0..74E3: contact.rig.probes[3].radius
        zero(v.contact[slot].rig.probes[3].radius);
        // 74E4..74F3: contact.rig.probes[4].local
        zero(v.contact[slot].rig.probes[4].local);
        // 74F4..7503: contact.rig.probes[4].current
        zero(v.contact[slot].rig.probes[4].current);
        // 7504..7513: contact.rig.probes[4].previous
        zero(v.contact[slot].rig.probes[4].previous);
        // 7514..7523: contact.rig.probes[4].velocity
        zero(v.contact[slot].rig.probes[4].velocity);
        // 7524..7527: contact.rig.probes[4].contact_iterations
        zero(v.contact[slot].rig.probes[4].contact_iterations);
        // 7528..752B: contact.rig.probes[4].mass_fraction
        zero(v.contact[slot].rig.probes[4].mass_fraction);
        // 752C..752F: contact.rig.probes[4].radius
        zero(v.contact[slot].rig.probes[4].radius);
        // 7530..753F: contact.rig.probes[5].local
        zero(v.contact[slot].rig.probes[5].local);
        // 7540..754F: contact.rig.probes[5].current
        zero(v.contact[slot].rig.probes[5].current);
        // 7550..755F: contact.rig.probes[5].previous
        zero(v.contact[slot].rig.probes[5].previous);
        // 7560..756F: contact.rig.probes[5].velocity
        zero(v.contact[slot].rig.probes[5].velocity);
        // 7570..7573: contact.rig.probes[5].contact_iterations
        zero(v.contact[slot].rig.probes[5].contact_iterations);
        // 7574..7577: contact.rig.probes[5].mass_fraction
        zero(v.contact[slot].rig.probes[5].mass_fraction);
        // 7578..757B: contact.rig.probes[5].radius
        zero(v.contact[slot].rig.probes[5].radius);
        // 757C..7613: source_uninterpreted.offset_757c
        zero(v.source_uninterpreted[slot].offset_757c);
        // 7614..7617: presentation.idle_hold
        zero(v.presentation[slot].idle_hold);
        // 7618..761B: presentation.idle_interval
        zero(v.presentation[slot].idle_interval);
        // 761C..761F: particles.field_761c
        zero(v.particles[slot].field_761c);
        // 7620..7623: particles.field_7620
        zero(v.particles[slot].field_7620);
        // 7624..7627: source_uninterpreted.offset_7624
        zero(v.source_uninterpreted[slot].offset_7624);
        // 7628..762B: particles.field_7628
        zero(v.particles[slot].field_7628);
        // 762C..762F: physics.mass_override
        zero(v.physics[slot].mass_override);
        // 7630..7633: contact.surface.saved_controller_mode
        zero(v.contact[slot].surface.saved_controller_mode);
        // 7634..7637: race.opposing_direction_ticks
        zero(v.race[slot].opposing_direction_ticks);
        // 7638..763B: race.unsafe_contact_ticks
        zero(v.race[slot].unsafe_contact_ticks);
        // 763C..763F: race.respawn_requested
        zero(v.race[slot].respawn_requested);
        // 7640..7643: race.collision_hold
        zero(v.race[slot].collision_hold);
        // 7644..7647: race.input_hold
        zero(v.race[slot].input_hold);
        // 7648..764B: physics.special_mode
        zero(v.physics[slot].special_mode);
        // 764C..764F: contact.surface.special_ticks
        zero(v.contact[slot].surface.special_ticks);
        // 7650..7653: rail.progress
        zero(v.rail[slot].progress);
        // 7654..7657: contact.surface.saved_2278
        zero(v.contact[slot].surface.saved_2278);
        // 7658..7663: contact.surface.special_accumulator
        zero(v.contact[slot].surface.special_accumulator);
        // 7664..766F: rail.projected_position
        zero(v.rail[slot].projected_position);
        // 7670..7673: source_uninterpreted.offset_7670
        zero(v.source_uninterpreted[slot].offset_7670);
        // 7674..7677: contact.surface.saved_24a4
        zero(v.contact[slot].surface.saved_24a4);
        // 7678..767B: race.event7678
        zero(v.race[slot].event7678);
        // 767C..767F: race.automatic_drive
        zero(v.race[slot].automatic_drive);
        // 7680..7683: race.pending_reset
        zero(v.race[slot].pending_reset);
        // 7684..7687: model_effects.route_envelope
        zero(v.model_effects[slot].route_envelope);
        // 7688..768B: race.start_countdown
        zero(v.race[slot].start_countdown);
        // 768C..76EF: startup.fields_768c
        zero(v.startup[slot].fields_768c);
        // 76F0..76F3: race.notice_active
        zero(v.race[slot].notice_active);
        // 76F4..76F7: race.notice_ticks
        zero(v.race[slot].notice_ticks);
        // 76F8..76FB: contact.direction_magnitude
        zero(v.contact[slot].direction_magnitude);
        // 76FC..76FF: tick.start_boost_ticks
        zero(v.tick[slot].start_boost_ticks);
        // 7700..7703: physics.stunt.inversion_latch
        zero(v.physics[slot].stunt.inversion_latch);
        // 7704..7707: tick.low_energy_notice
        zero(v.tick[slot].low_energy_notice);
        // 7708..770B: load.sounds.horn
        zero(v.load[slot].sounds.horn);
        // 770C..770F: load.sounds.engine_rev
        zero(v.load[slot].sounds.engine_rev);
        // 7710..7713: load.sounds.engine_idle
        zero(v.load[slot].sounds.engine_idle);
        // 7714..7717: load.sounds.engine_boost
        zero(v.load[slot].sounds.engine_boost);
        // 7718..771B: audio.field_7718
        zero(v.audio[slot].field_7718);
        // 771C..771F: audio.field_771c
        zero(v.audio[slot].field_771c);
        // 7720..7723: audio.field_7720
        zero(v.audio[slot].field_7720);
    }
}
}

void clear_source_cold_vehicle_records_40a305(RaceSessionState& state) noexcept {
    clear_source_records(state); //first six contiguous source strides
    auto& v=state.vehicles;
    for(unsigned slot=6;slot!=8;++slot){
        zero(v.configuration_identity[slot].header); //0000..001F
        zero(v.cold_tail[slot-6].offset_0020); //0020..201B
        zero(v.presentation[slot].wheel_centers[0]); //201C..202B
        zero(v.cold_tail[slot-6].offset_202c); //202C..206F
        zero(v.wheels[slot][0].previous_angle); //2070..2073
        zero(v.wheels[slot][0].current_angle); //2074..2077
        zero(v.cold_tail[slot-6].offset_2078); //2078..20B3
        zero(v.presentation[slot].wheel_centers[1]); //20B4..20C3
        zero(v.cold_tail[slot-6].offset_20c4); //20C4..2107
        zero(v.wheels[slot][1].previous_angle); //2108..210B
        zero(v.wheels[slot][1].current_angle); //210C..210F
        zero(v.cold_tail[slot-6].offset_2110); //2110..214B
        zero(v.presentation[slot].wheel_centers[2]); //214C..215B
        zero(v.cold_tail[slot-6].offset_215c); //215C..219F
        zero(v.wheels[slot][2].previous_angle); //21A0..21A3
        zero(v.wheels[slot][2].current_angle); //21A4..21A7
        zero(v.cold_tail[slot-6].offset_21a8); //21A8..21E3
        zero(v.presentation[slot].wheel_centers[3]); //21E4..21F3
        zero(v.cold_tail[slot-6].offset_21f4); //21F4..2237
        zero(v.wheels[slot][3].previous_angle); //2238..223B
        zero(v.wheels[slot][3].current_angle); //223C..223F
        zero(v.cold_tail[slot-6].offset_2240); //2240..26AF
        zero(v.models[slot].model); //26B0..26B3
        zero(v.models[slot].shadow); //26B4..26B7
        zero(v.models[slot].position); //26B8..26C7
        zero(v.models[slot].scale); //26C8..26D7
        zero(v.models[slot].target); //26D8..26E7
        zero(v.models[slot].up); //26E8..26F7
        zero(v.models[slot].template_tail); //26F8..2707
        zero(v.models[slot].field_2708); //2708..270B
        zero(v.models[slot].body_rotation); //270C..274B
        zero(v.wheels[slot][0].rotation); //274C..278B
        zero(v.wheels[slot][1].rotation); //278C..27CB
        zero(v.wheels[slot][2].rotation); //27CC..280B
        zero(v.wheels[slot][3].rotation); //280C..284B
        zero(v.cold_tail[slot-6].offset_284c); //284C..3133
        zero(v.presentation[slot].field_3134); //3134..3137
        zero(v.cold_tail[slot-6].offset_3138); //3138..33C7
        zero(v.presentation[slot].field_33c8); //33C8..33CB
        zero(v.cold_tail[slot-6].offset_33cc); //33CC..3507
        zero(v.presentation[slot].field_3508); //3508..350B
        zero(v.cold_tail[slot-6].offset_350c); //350C..3923
        zero(v.presentation[slot].field_3924); //3924..3927
        zero(v.presentation[slot].field_3928); //3928..392B
        zero(v.cold_tail[slot-6].offset_392c); //392C..7613
        zero(v.presentation[slot].idle_hold); //7614..7617
        zero(v.presentation[slot].idle_interval); //7618..761B
        zero(v.cold_tail[slot-6].offset_761c); //761C..7723
    }
}

SourceVehicleClear444dc9Impl::SourceVehicleClear444dc9Impl(RaceSessionPlatform& platform)
    :platform_(platform) {
    platform_.require_owner_thread();
}

void SourceVehicleClear444dc9Impl::clear_444dc9(RaceSessionPlatform& platform) {
    // Host checks precede the first source effect; state access itself is noexcept.
    platform_.require_owner_thread();
    if(&platform!=&platform_)
        throw std::invalid_argument("444DC9 clear belongs to another source platform");
    clear_source_records(platform_.state());
}
}
