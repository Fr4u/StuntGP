#pragma once
#include "sgn/assets.hpp"
#include "sgn/vehicle_math.hpp"
#include <array>
#include <cstdint>

namespace sgn::retail
{
// Wheel order and numerical units stay in the retail coordinate system.
// These are recovered kernels, not yet a complete vehicle tick/contact solver.
using Curve = std::array<float, 128>;
struct Wheel
{
    float acceleration{}, speed{}, slip{}, load{}, reference_load{}, lateral_speed{};
    float rolling_force{}, grip_usage{}, static_friction{}, kinetic_friction{};
    std::int32_t contact_countdown{};
    float surface_grip{}, surface_roughness{}, surface_rolling{};
    float braking_signal{};
    VehicleVector contact_normal{};
};
struct TyreParameters
{
    float radius{}, pressure{}, static_friction{}, kinetic_friction{};
    float travel_in{}, damping_in{}, stiffness_in{}, travel_out{}, damping_out{}, stiffness_out{};
};
struct VehicleParameters
{
    Curve friction_curve{}, steering_curve{};
    float mass{}, com_a{}, com_b{}, com_height{}, wheelbase{}, track_front{}, track_rear{};
    float length{}, width{}, height{}, ride_height{};
    float brake_bias{}, brake_acceleration{}, steering_max{}, steering_acceleration{}, steering_sensitivity{};
    float rpm_max{}, rpm_idle{}, rpm_limit{}, engine_decay{}, max_torque{};
    std::array<Curve, 4> torque_curves{};
    std::int32_t drive_mode{}, gear_count{}, manual_gears{};
    float final_drive{}, shift_time{}, efficiency{}, power_split{};
    std::array<float, 8> ratios{};
    float drag_coefficient{}, lift_coefficient{}, frontal_area{};
    std::array<TyreParameters, 4> tyres{};
    float turbo_acceleration{}, turbo_max{}, turbo_decay{}, turbo_scale{};
    Curve turbo_curve{};
    float turbo_energy_threshold{}, turbo_consumption{}, fuel_capacity{}, fuel_unit_mass{}, fuel_consumption{};
    Curve consumption_curve{};
    float refuel_rate{};
    Curve digital_steering{}, digital_brake{}, analog_steering{}, analog_brake{}, velocity_steering{};
    float launch_time{}, launch_tolerance{}, launch_speed{}, max_pitch_speed{}, max_yaw_speed{};
};
VehicleParameters decode_vehicle_parameters(std::string_view text);
struct VehicleInput
{
    std::uint8_t throttle{}, brake{}, left{}, right{}, shift{}, stunt{}, rear_lock{}, steering_boost{}, turbo{},
        auxiliary{};
};
// Eight effect storage words reset by 004128F0. Interpretation by rendering
// remains separate; no original pointer or object layout enters the runtime.
struct VehicleCameraEffects
{
    std::array<std::uint32_t, 8> values{};
};
struct VehicleStatistic
{
    std::uint32_t count{};
    float last{}, total{}, average{};
};
struct VehicleStuntState
{
    std::array<std::int32_t, 4> phase_indices{};
    // Four 64-word histories share their boundary with the next history and
    // then four wheel-balance counters. Index 64 is a defined retail write.
    std::array<std::uint32_t, 260> history_and_balance{};
    std::uint32_t flip_phase{}, flip_timer{};
    VehicleVector flip_up{};
    std::uint32_t drift_ticks{}, slipstream_ticks{}, roof_ticks{}, inversion_timer{}, inversion_latch{};
    std::int32_t active{}, previous_active{}, previous_contact_counter{};
    VehicleVector accumulated_angles{}, angular_velocity{}, angular_acceleration{};
    std::uint32_t launch_tick{};
    float launch_height{}, maximum_height{};
    std::uint32_t height_level{}, launch_state{}, landing_pending{}, landing_counter{}, landing_hold{}, score_hold{};
    std::uint32_t chain_count{}, displayed_chain_count{}, chain_decay_active{}, chain_decay_timer{};
};
struct VehicleState
{
    VehicleVector position{}, previous_position{};
    VehicleMatrix orientation{}, incremental{}, secondary_rotation{};
    VehicleVector basis_x{}, basis_z{};
    float basis_y_w{}, velocity_w{}, previous_velocity_w{};
    Vec3 previous_velocity{}, acceleration{};
    Vec3 velocity{}, up{};
    float total_speed{}, speed{}, steering{}, throttle{}, rpm{}, engine_output{}, engine_output_scale{};
    Curve engine_output_curve{};
    float torque{};
    std::int32_t torque_curve{}, gear{};
    float clutch{};
    std::int32_t downshift_ticks{};
    float drag_force{}, lift_force{}, drive_force{}, air_factor{};
    std::int32_t previous_contact_class{}, contact_class{}, contact_hold{};
    std::array<Wheel, 4> wheels{};
    std::int32_t turbo_active{};
    float turbo_rpm{}, turbo_force{}, energy{}, energy_capacity{};
    float fuel{};
    float score{}, air_score_rate{}, launch_score{};
    std::array<VehicleStatistic, 4> stunt_statistics{}; // launch, landing, points, height
    std::int32_t launch_effect_level{};
    float launch_effect_strength{};
    std::uint32_t launch_effect_timer{}, launch_event{};
    float launch_event_strength{};
    std::uint32_t stunt_meter{};
    std::array<std::int32_t, 4> control_flags{};
    VehicleInput input;
    std::uint8_t previous_shift{}, previous_stunt{};
    std::int32_t contact_counter{}, brake_ramp{}, steering_ramp{}, brake_signal{};
    std::uint32_t airborne_ticks{};
    float steering_boost{};
    VehicleCameraEffects camera_effects;
    VehicleStuntState stunt;
    std::int32_t mass_override{};
    float performance_multiplier{};
    VehicleVector auxiliary_displacement{}, previous_auxiliary_displacement{}, auxiliary_velocity{};
    std::int32_t gear_changed{}, energy_exempt{}, upgrade_active{};
    float upgrade_condition{};
    float effective_mass{}, effective_com_height{}, average_grip{};
    std::array<float, 4> suspension{};
    float body_heave{}, body_pitch{}, body_roll{}, average_load{};
    VehicleVector front_axle_position{}, rear_axle_position{}, front_axle_velocity{}, rear_axle_velocity{};
    std::int32_t special_mode{};
};
struct EngineContext
{
    std::int32_t game_mode{};
    bool automatic_shift_allowed{}; // player 767c == 1 || player 7688 < 61
    float engine_consumption_modifier{}, turbo_consumption_modifier{};
};

// Each implementation cites its retail entry; exact reviewed spans live in
// evidence/reviewed. Host inputs must be finite and satisfy parameter contracts.
void distribute_drive(const VehicleParameters &, VehicleState &, float acceleration); // 0040e3a0
void engage_clutch(const VehicleParameters &, VehicleState &);                        // 0040e470
double driven_mass(const VehicleParameters &, const VehicleState &, float mass);      // 0040e710
void update_turbo(const VehicleParameters &, VehicleState &);                         // 0040ead0
void limit_reverse_rpm(const VehicleParameters &, VehicleState &);                    // 0040eba0
float limit_low_energy(const VehicleState &, float acceleration);                     // 0040d650
void update_reference_load(VehicleState &, float gravity);                            // 0040f3c0
void rolling_resistance(const VehicleParameters &, VehicleState &, unsigned wheel, float longitudinal_speed,
                        float &axle_force); // 0040f3f0
void aerodynamic_drag(const VehicleParameters &, VehicleState &, float &front, float &rear,
                      float special_drag);                          // 0040f4a0 + 00405860
void attenuate_steering(const VehicleParameters &, VehicleState &); // 0040f570
void tyre_friction(const VehicleParameters &, VehicleState &, unsigned wheel, float &lateral,
                   float &longitudinal);                                                // 0040f5b0
void average_grip(VehicleState &);                                                      // 0040f730
void airborne_wheel_decay(VehicleState &);                                              // 0040fb00
void classify_contacts(VehicleState &);                                                 // 0040fb30
void aerodynamic_lift(const VehicleParameters &, VehicleState &);                       // 0040edd0
void clear_wheel_acceleration(VehicleState &);                                          // 00413450
void stop_wheel(VehicleState &, unsigned wheel);                                        // 00413430
void build_engine_curve(const VehicleParameters &, VehicleState &);                     // 0040a1d0
void shift_up(const VehicleParameters &, VehicleState &);                               // 0040c170 + 0040c160
void shift_down(const VehicleParameters &, VehicleState &);                             // 0040c1a0 + 0040c160
void shift_neutral(VehicleState &);                                                     // 0040c1f0 + 0040c160
void automatic_shift(const VehicleParameters &, VehicleState &, const EngineContext &); // 0040e550 + 0040e4f0
void consume_engine_energy(const VehicleParameters &, VehicleState &, const EngineContext &,
                           float rpm); // 0040ec00 + 00405800
void consume_turbo_energy(const VehicleParameters &, VehicleState &, const EngineContext &,
                          float rpm); // 0040eca0 + 004057e0
void engine_tick(VehicleParameters &, VehicleState &,
                 const EngineContext &);                                   // 0040e7a0, including its full callee chain
void digital_throttle(VehicleState &);                                     // 0040cb90
void analog_throttle(VehicleState &);                                      // 0040c220
void digital_brake(const VehicleParameters &, VehicleState &);             // 0040cc90, including its full callee chain
void analog_brake(const VehicleParameters &, VehicleState &);              // 0040c320, including its full callee chain
void wheel_braking_signal(VehicleState &, unsigned wheel);                 // 00414ac0
void brake_signal(VehicleState &) noexcept;                                // 00415630
void steering_boost(VehicleState &);                                       // 0040ce60
void digital_steering(const VehicleParameters &, VehicleState &);          // 0040ced0
void reversed_digital_steering(const VehicleParameters &, VehicleState &); // 0040cf90
void digital_manual_shift(const VehicleParameters &, VehicleState &);      // 0040d050
void analog_manual_shift(const VehicleParameters &, VehicleState &);       // 0040c6c0
void reset_camera_dependent_effects(VehicleState &) noexcept;              // 004128f0 and all four callees
void reset_balance_counters(VehicleState &) noexcept;                      // 00410dd0
void reset_flip_state(VehicleState &target, VehicleState &current) noexcept;  // 00410f50
void reset_drift_counter(VehicleState &) noexcept;                            // 00411140
void reset_slipstream_counter(VehicleState &) noexcept;                       // 00411280
void reset_roof_counter(VehicleState &) noexcept;                             // 00411410
void reset_inversion_state(VehicleState &) noexcept;                          // 00411330
void reset_phase_history(VehicleState &) noexcept;                            // 004118b0
void reset_air_rotation(VehicleState &) noexcept;                             // 00412430
void reset_stunt_state(VehicleState &target, VehicleState &current) noexcept; // 00411490
inline void reset_stunt_state(VehicleState &s) noexcept
{
    reset_stunt_state(s, s);
}
void reset_stunt_score_hold(VehicleState &) noexcept;                                                       // 00411880
void begin_stunt_landing(VehicleState &) noexcept;                                                          // 00411890
void reset_launch_state(VehicleState &) noexcept;                                                           // 00412260
void begin_stunt_height(VehicleState &) noexcept;                                                           // 004115c0
void negative_pitch_input(const VehicleParameters &, VehicleState &);                                       // 0040bf20
void positive_pitch_input(const VehicleParameters &, VehicleState &);                                       // 0040bfb0
void positive_yaw_input(const VehicleParameters &, VehicleState &);                                         // 0040c040
void negative_yaw_input(const VehicleParameters &, VehicleState &);                                         // 0040c0d0
void clear_angular_acceleration(VehicleState &) noexcept;                                                   // 0040c4e0
void assist_air_pitch(VehicleState &, Vec3 unit_gravity);                                                   // 00414940
void assist_air_yaw(VehicleState &);                                                                        // 004149e0
void digital_stunt_input(const VehicleParameters &, VehicleState &, std::uint32_t tick, Vec3 unit_gravity); // 0040d090
void analog_stunt_input(const VehicleParameters &, VehicleState &, std::uint32_t tick, Vec3 unit_gravity);  // 0040c5f0
void save_phase_history(VehicleState &);                                                                    // 004118d0
void update_phase_history(VehicleState &, Vec3 unit_gravity);                                               // 00411950
void stop_all_wheels(VehicleState &) noexcept;                                                              // 00413470
void reset_vehicle_motion(VehicleState &) noexcept;                               // 00413490 and both identity callees
void accumulate_air_score(VehicleState &, std::int32_t game_mode);                // 004114e0
void update_stunt_chain_decay(VehicleState &) noexcept;                           // 004121c0
void steer_air_velocity(VehicleState &, Vec3 unit_gravity);                       // 00412270
void record_stunt_statistic(VehicleStatistic &, float value);                     // 0041d1f0
void record_stunt_height(VehicleState &);                                         // 0041d230
void set_launch_effect(VehicleState &, float strength);                           // 004163e0
void launch_stunt(const VehicleParameters &, VehicleState &, std::uint32_t tick); // 004116f0
void reset_stunt_meter(VehicleState &) noexcept;                                  // 00406ff0
void fail_stunt_landing(VehicleState &);                                          // 00411fa0
void suspension_pose(const VehicleParameters &, VehicleState &);                  // 0040ee70
void suspension_tick(const VehicleParameters &, VehicleState &, std::uint32_t tick, VehicleRandom &); // 0040efe0
void acceleration_tilt(const VehicleParameters &, VehicleState &);                                    // 0040f250
struct VehicleSupport
{
    VehicleVector body_position{};
    std::array<VehicleVector, 4> local{}, current{};
};
void prepare_physical_parameters(const VehicleParameters &, VehicleState &); // 00418580
void update_support_basis(VehicleState &, const VehicleSupport &);           // 00414710
void approach_body_speed(VehicleState &);                                    // 00406270
void damp_auxiliary_motion(VehicleState &);                                  // 00413330
void support_rotation(VehicleVector body_position, VehicleVector probe_position, float angle,
                      VehicleMatrix &);                                                             // 0040f760
void wheel_rotation(VehicleState &, const VehicleSupport &, unsigned wheel, float inverse_gravity); // 0040f880
void bank_displacement(const VehicleParameters &, VehicleState &, Vec3 gravity);                    // 0040fbb0
void apply_gravity(Vec3 &velocity, Vec3 gravity, float scale);                                      // 00401d70
struct VehicleDynamicsContext
{
    EngineContext engine;
    float gravity_magnitude{}, inverse_gravity{};
    Vec3 gravity{};
    float special_drag{};
    std::uint32_t tick{};
};
// Exact prefix 0040FC90..00410759. Airborne/stunt/trail tail remains separate;
// this is not a complete retail vehicle update or a stand-alone game loop.
void vehicle_dynamics_core(VehicleParameters &, VehicleState &, const VehicleSupport &, const VehicleDynamicsContext &,
                           VehicleRandom &);
} // namespace sgn::retail
