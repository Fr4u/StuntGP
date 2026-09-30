#include "sgn/source_cold_vehicle_state.hpp"
#include "sgn/source_vehicle_clear.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/race_session.hpp"
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>
extern "C" std::uint32_t sgn_cold_template_40a3e9(const void *, void *const *, const void *, std::uint32_t) noexcept;
extern "C" void sgn_cold_torque_40a160(float *) noexcept;
extern "C" void sgn_cold_turbo_40a71b(float *) noexcept;
extern "C" void sgn_cold_launch_40a78b(const float *, float *) noexcept;
extern "C" void sgn_cold_friction_40a82d(float *) noexcept;
extern "C" void sgn_source_config_engine_40a1d0(const float *, const float *, const float *, const float *, float *,
                                                float *);
namespace sgn::retail
{
std::span<const std::uint8_t, 19> SourceApplicationLiteralRef::cold_vehicle_name_472c60() const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(0x40a41d, 0x472c60, "cold vehicle name App owner expired");
    identity->owner->require_owner_thread();
    return identity->owner->cold_vehicle_name_472c60_;
}
std::span<std::uint8_t, 19> SourceApplicationLiteralData::mutable_cold_vehicle_name_472c60() &
{
    require_owner_thread();
    return cold_vehicle_name_472c60_;
}
namespace
{
using W = std::uint32_t;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(ModelTransform) == 80 && sizeof(VehicleVector) == 16 && sizeof(VehicleMatrix) == 64);
static_assert(std::is_standard_layout_v<ModelTransform> && sizeof(Vec3) == 12);
static_assert(offsetof(ModelTransform, position) == 0 && offsetof(ModelTransform, scale) == 16);
static_assert(offsetof(ModelTransform, forward) == 32 && offsetof(ModelTransform, up) == 48 &&
              offsetof(ModelTransform, tail) == 64);
// Only proven numeric/byte-array source owners use this bit-preserving store.
// No optional pointer, host padding or owning object is serialized.
template <std::size_t Offset, class T> void put(T &owner, W bits) noexcept
{
    static_assert(std::is_trivially_copyable_v<T> && Offset + 4 <= sizeof(T));
    std::memcpy(reinterpret_cast<std::uint8_t *>(&owner) + Offset, &bits, 4);
}
void identity(VehicleMatrix &matrix) noexcept
{
    for (unsigned lane = 0; lane != 16; ++lane)
        matrix[lane] = std::bit_cast<float>(lane % 5 == 0 ? 0x3f800000u : 0u);
}
unsigned current(RaceSessionState &w, W site)
{
    if (w.current_slot >= 6)
        throw SourceAllocationError(site, "40A2F0 current vehicle leaves its six physical owners");
    return w.current_slot;
}
void admit(RaceSessionPlatform &platform, W site)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(site, "cold vehicle state requires the actual source platform");
}
} // namespace
void initialize_source_cold_vehicle_state_40a2f0(RaceSessionPlatform &platform)
{
    admit(platform, 0x40a2f0);
    auto &w = platform.state();
    auto &v = w.vehicles;
    clear_source_cold_vehicle_records_40a305(w); // 0xEE48 DWORDs =8*0x7724 bytes, DF=0
    w.current_slot = 0;                          // 40A30C, after all eight cleared strides
    unsigned id = 0;
    put<0>(v.race[id].active, 0x00000001u); // 40A316: +1BA8
    identity(v.physics[0].orientation);     // 40A320 ->44EE10, absolute49BDC4
    id = current(w, 0x40a325);
    identity(v.physics[id].incremental); // 40A330
    id = current(w, 0x40a335);
    identity(v.physics[id].secondary_rotation); // 40A341
    id = current(w, 0x40a346);
    const auto shadow = platform.source_scene_resources().model_handle(5); // 40A34D:636468 pointer capture only
    const auto model = platform.source_scene_resources().model_handle(4);  // 40A353:636458; no PMD payload preflight
    put<0>(v.physics[id].position, 0x00000000u);                           // 40A359: +1BAC
    put<4>(v.physics[id].position, 0x00000000u);                           // 40A35F: +1BB0
    put<8>(v.physics[id].position, 0x00000000u);                           // 40A365: +1BB4
    put<0>(v.physics[id].acceleration, 0x00000000u);                       // 40A36B: +1CAC
    put<4>(v.physics[id].acceleration, 0x00000000u);                       // 40A371: +1CB0
    put<8>(v.physics[id].acceleration, 0x00000000u);                       // 40A377: +1CB4
    put<0>(v.physics[id].velocity, 0x00000000u);                           // 40A37D: +1C8C
    put<4>(v.physics[id].velocity, 0x00000000u);                           // 40A383: +1C90
    put<8>(v.physics[id].velocity, 0x00000000u);                           // 40A389: +1C94
    put<0>(v.physics[id].previous_velocity, 0x00000000u);                  // 40A38F: +1C9C
    put<4>(v.physics[id].previous_velocity, 0x00000000u);                  // 40A395: +1CA0
    put<8>(v.physics[id].previous_velocity, 0x00000000u);                  // 40A39B: +1CA4
    put<0>(v.camera_settings[id].angles, 0x00000000u);                     // 40A3A1: +22EC
    put<8>(v.camera_settings[id].angles, 0x00000000u);                     // 40A3A7: +22F4
    put<4>(v.camera_settings[id].angles, 0x43340000u);                     // 40A3AD: +22F0
    put<0>(v.camera_settings[id].distance, 0x447a0000u);                   // 40A3B7: +22FC
    v.models[0].shadow = shadow;                                           // 40A3C1, absolute49C8AC
    v.models[0].model = model;                                             // 40A3CD, absolute49C8A8
    identity(v.models[id].body_rotation);                                  // 40A3D3 ->40A2C0 ->44EE10
    for (auto &wheel : v.wheels[id])
        identity(wheel.rotation); // 40A2D3, exactly four successive matrices
    auto &dst = v.models[0];
    // Actual twenty source destinations. No contiguous native ModelStorage ABI assumed.
    std::array<void *, 20> lanes{&dst.position.x,
                                 &dst.position.y,
                                 &dst.position.z,
                                 &dst.position.w,
                                 &dst.scale.x,
                                 &dst.scale.y,
                                 &dst.scale.z,
                                 &dst.scale.w,
                                 &dst.target.x,
                                 &dst.target.y,
                                 &dst.target.z,
                                 &dst.target.w,
                                 &dst.up.x,
                                 &dst.up.y,
                                 &dst.up.z,
                                 &dst.up.w,
                                 &dst.template_tail[0],
                                 &dst.template_tail[1],
                                 &dst.template_tail[2],
                                 &dst.template_tail[3]};
    const auto app = platform.source_application_literals();
    const auto name = app.cold_vehicle_name_472c60(); // host lifetime/extent borrow only; bytes read at40A41D
    const auto length =
        sgn_cold_template_40a3e9(&w.model_draws.template_pose, lanes.data(), name.data(), static_cast<W>(name.size()));
    if (length == 0)
        throw SourceLiteralReadError(0x40a41d, 0x472c73,
                                     "cold name scan reaches bytes beyond its represented App owner");
    id = current(w, 0x40a42b);
    unsigned copied = 0;
    for (; copied + 4 <= length; copied += 4)
    {
        W word;
        std::memcpy(&word, name.data() + copied, 4);
        std::memcpy(v.configuration_identity[id].header.data() + copied, &word, 4);
    } // 40A43E
    for (; copied < length; ++copied)
        v.configuration_identity[id].header[copied] = static_cast<char>(name[copied]); // 40A44A
    put<0>(v.parameters[id].mass, 0x447a0000u);                                        // 40A44C: +0220
    put<0>(v.parameters[id].com_a, 0x3f000000u);                                       // 40A456: +0224
    put<0>(v.parameters[id].com_b, 0x3f000000u);                                       // 40A45C: +0228
    put<0>(v.parameters[id].com_height, 0x41f00001u);                                  // 40A462: +022C
    put<0>(v.parameters[id].wheelbase, 0x43740000u);                                   // 40A471: +0230
    put<0>(v.parameters[id].track_front, 0x43180000u);                                 // 40A47B: +0234
    put<0>(v.parameters[id].track_rear, 0x43180000u);                                  // 40A481: +0238
    put<0>(v.parameters[id].length, 0x43c80000u);                                      // 40A487: +023C
    put<0>(v.parameters[id].width, 0x43180000u);                                       // 40A491: +0240
    put<0>(v.parameters[id].height, 0x42e60000u);                                      // 40A497: +0244
    put<0>(v.parameters[id].steering_max, 0x41c80000u);                                // 40A4A6: +024C
    put<0>(v.parameters[id].steering_acceleration, 0x3f800000u);                       // 40A4BB: +0250
    for (auto &value : v.parameters[id].steering_curve)
        value = std::bit_cast<float>(0x3f800000u);               // 40A4C7 REP128
    put<0>(v.parameters[id].brake_acceleration, 0x40200000u);    // 40A4C9: +0D58
    put<0>(v.parameters[id].brake_bias, 0x3ecccccdu);            // 40A4D3: +0D5C
    put<0>(v.physics[id].brake_signal, 0x000000ffu);             // 40A4DD: +28A8
    put<0>(v.initialization_extras[id].field_28b0, 0xc2700000u); // 40A4E7: +28B0
    put<0>(v.source_uninterpreted[id].offset_28ac, 0x00000000u); // 40A4F3: +28AC
    put<0>(v.physics[id].wheels[0].braking_signal, 0x00000000u); // 40A4F9: +2CF4
    put<0>(v.physics[id].wheels[1].braking_signal, 0x00000000u); // 40A506: +2CF8
    put<0>(v.physics[id].wheels[2].braking_signal, 0x00000000u); // 40A50C: +2CFC
    put<0>(v.physics[id].wheels[3].braking_signal, 0x00000000u); // 40A512: +2D00
    put<0>(v.parameters[id].drag_coefficient, 0x3f19999au);      // 40A518: +0CAC
    put<0>(v.parameters[id].lift_coefficient, 0x3e99999au);      // 40A522: +0CB0
    put<0>(v.parameters[id].frontal_area, 0x3fee147bu);          // 40A52C: +0CB4
    put<0>(v.physics[id].air_factor, 0x3f800000u);               // 40A536: +1FD0
    {
        const auto gravity_bits = std::bit_cast<W>(w.gravity.magnitude); // 40A548 live482670 reload
        put<0>(v.parameters[id].tyres[0].radius, 0x3ea8f5c3u);           // 40A54E: +0CB8
        put<0>(v.parameters[id].tyres[0].pressure, 0x41f00000u);         // 40A555: +0CBC
        put<0>(v.physics[id].wheels[0].load, gravity_bits);              // 40A55B: +1FEC
        put<0>(v.parameters[id].tyres[0].static_friction, 0x3f333333u);  // 40A55D: +0CC0
        put<0>(v.parameters[id].tyres[0].kinetic_friction, 0x3ecccccdu); // 40A569: +0CC4
        put<0>(v.parameters[id].tyres[0].travel_in, 0x41200000u);        // 40A570: +0CF8
        put<0>(v.parameters[id].tyres[0].damping_in, 0x3f4ccccdu);       // 40A573: +0CFC
        put<0>(v.parameters[id].tyres[0].stiffness_in, 0x3ba3d70au);     // 40A579: +0D00
        put<0>(v.parameters[id].tyres[0].travel_out, 0x41200000u);       // 40A580: +0D04
        put<0>(v.parameters[id].tyres[0].damping_out, 0x3f4ccccdu);      // 40A586: +0D08
        put<0>(v.parameters[id].tyres[0].stiffness_out, 0x3ba3d70au);    // 40A58D: +0D0C
    }
    {
        const auto gravity_bits = std::bit_cast<W>(w.gravity.magnitude); // 40A548 live482670 reload
        put<0>(v.parameters[id].tyres[1].radius, 0x3ea8f5c3u);           // 40A54E: +0CC8
        put<0>(v.parameters[id].tyres[1].pressure, 0x41f00000u);         // 40A555: +0CCC
        put<0>(v.physics[id].wheels[1].load, gravity_bits);              // 40A55B: +2084
        put<0>(v.parameters[id].tyres[1].static_friction, 0x3f333333u);  // 40A55D: +0CD0
        put<0>(v.parameters[id].tyres[1].kinetic_friction, 0x3ecccccdu); // 40A569: +0CD4
        put<0>(v.parameters[id].tyres[1].travel_in, 0x41200000u);        // 40A570: +0D10
        put<0>(v.parameters[id].tyres[1].damping_in, 0x3f4ccccdu);       // 40A573: +0D14
        put<0>(v.parameters[id].tyres[1].stiffness_in, 0x3ba3d70au);     // 40A579: +0D18
        put<0>(v.parameters[id].tyres[1].travel_out, 0x41200000u);       // 40A580: +0D1C
        put<0>(v.parameters[id].tyres[1].damping_out, 0x3f4ccccdu);      // 40A586: +0D20
        put<0>(v.parameters[id].tyres[1].stiffness_out, 0x3ba3d70au);    // 40A58D: +0D24
    }
    {
        const auto gravity_bits = std::bit_cast<W>(w.gravity.magnitude); // 40A548 live482670 reload
        put<0>(v.parameters[id].tyres[2].radius, 0x3ea8f5c3u);           // 40A54E: +0CD8
        put<0>(v.parameters[id].tyres[2].pressure, 0x41f00000u);         // 40A555: +0CDC
        put<0>(v.physics[id].wheels[2].load, gravity_bits);              // 40A55B: +211C
        put<0>(v.parameters[id].tyres[2].static_friction, 0x3f333333u);  // 40A55D: +0CE0
        put<0>(v.parameters[id].tyres[2].kinetic_friction, 0x3ecccccdu); // 40A569: +0CE4
        put<0>(v.parameters[id].tyres[2].travel_in, 0x41200000u);        // 40A570: +0D28
        put<0>(v.parameters[id].tyres[2].damping_in, 0x3f4ccccdu);       // 40A573: +0D2C
        put<0>(v.parameters[id].tyres[2].stiffness_in, 0x3ba3d70au);     // 40A579: +0D30
        put<0>(v.parameters[id].tyres[2].travel_out, 0x41200000u);       // 40A580: +0D34
        put<0>(v.parameters[id].tyres[2].damping_out, 0x3f4ccccdu);      // 40A586: +0D38
        put<0>(v.parameters[id].tyres[2].stiffness_out, 0x3ba3d70au);    // 40A58D: +0D3C
    }
    {
        const auto gravity_bits = std::bit_cast<W>(w.gravity.magnitude); // 40A548 live482670 reload
        put<0>(v.parameters[id].tyres[3].radius, 0x3ea8f5c3u);           // 40A54E: +0CE8
        put<0>(v.parameters[id].tyres[3].pressure, 0x41f00000u);         // 40A555: +0CEC
        put<0>(v.physics[id].wheels[3].load, gravity_bits);              // 40A55B: +21B4
        put<0>(v.parameters[id].tyres[3].static_friction, 0x3f333333u);  // 40A55D: +0CF0
        put<0>(v.parameters[id].tyres[3].kinetic_friction, 0x3ecccccdu); // 40A569: +0CF4
        put<0>(v.parameters[id].tyres[3].travel_in, 0x41200000u);        // 40A570: +0D40
        put<0>(v.parameters[id].tyres[3].damping_in, 0x3f4ccccdu);       // 40A573: +0D44
        put<0>(v.parameters[id].tyres[3].stiffness_in, 0x3ba3d70au);     // 40A579: +0D48
        put<0>(v.parameters[id].tyres[3].travel_out, 0x41200000u);       // 40A580: +0D4C
        put<0>(v.parameters[id].tyres[3].damping_out, 0x3f4ccccdu);      // 40A586: +0D50
        put<0>(v.parameters[id].tyres[3].stiffness_out, 0x3ba3d70au);    // 40A58D: +0D54
    }
    put<0>(v.parameters[id].drive_mode, 0x00000000u);            // 40A5AB: +0C70
    put<0>(v.parameters[id].shift_time, 0x3f000000u);            // 40A5B1: +0CA0
    put<0>(v.parameters[id].gear_count, 0x00000006u);            // 40A5B7: +0C74
    put<0>(v.parameters[id].manual_gears, 0x00000000u);          // 40A5C1: +0C78
    put<0>(v.physics[id].gear, 0x00000001u);                     // 40A5CC: +1F10
    put<0>(v.physics[id].clutch, 0x3f800000u);                   // 40A5D6: +1F14
    put<0>(v.source_uninterpreted[id].offset_1f18, 0x00000000u); // 40A5DC: +1F18
    put<0>(v.parameters[id].ratios, 0xc003d70au);                // 40A5E2: +0C80
    put<4>(v.parameters[id].ratios, 0x00000000u);                // 40A5EC: +0C84
    put<8>(v.parameters[id].ratios, 0x40570a3du);                // 40A5F2: +0C88
    put<12>(v.parameters[id].ratios, 0x4003d70au);               // 40A5FC: +0C8C
    put<16>(v.parameters[id].ratios, 0x3fb0a3d7u);               // 40A606: +0C90
    put<20>(v.parameters[id].ratios, 0x3f851eb8u);               // 40A610: +0C94
    put<24>(v.parameters[id].ratios, 0x3f6b851fu);               // 40A61A: +0C98
    put<28>(v.parameters[id].ratios, 0x3f4ccccdu);               // 40A624: +0C9C
    put<0>(v.parameters[id].final_drive, 0x4078f5c3u);           // 40A62E: +0C7C
    put<0>(v.parameters[id].efficiency, 0x3f666666u);            // 40A638: +0CA4
    put<0>(v.race[id].control_kind, 0x00000002u);                // 40A642: +227C
    put<0>(v.physics[id].steering_ramp, 0x00000000u);            // 40A64C: +2298
    put<0>(v.physics[id].brake_ramp, 0x00000000u);               // 40A652: +229C
    for (unsigned at = 0; at != 128; ++at)
    { // 40A663..681, interleaved five source writes
        v.parameters[id].digital_steering[at] = 1;
        v.parameters[id].digital_brake[at] = 1;
        v.parameters[id].analog_steering[at] = 1;
        v.parameters[id].analog_brake[at] = 1;
        v.parameters[id].velocity_steering[at] = 1;
    }
    put<0>(v.parameters[id].rpm_max, 0x460ca000u);          // 40A683: +045C
    put<0>(v.parameters[id].rpm_limit, 0x45dac000u);        // 40A68D: +0464
    put<0>(v.parameters[id].rpm_idle, 0x43160000u);         // 40A697: +0460
    put<0>(v.parameters[id].engine_decay, 0x3f7851ecu);     // 40A6A1: +0468
    put<0>(v.physics[id].engine_output_scale, 0x43960000u); // 40A6AB: +1D04
    put<0>(v.parameters[id].max_torque, 0x43c50000u);       // 40A6B5: +046C
    {
        const auto reached = current(w, 0x40a160);
        sgn_cold_torque_40a160(v.parameters[reached].torque_curves[0].data());
    } // 40A6BF
    {
        const auto reached = current(w, 0x40a1d6);
        auto &state = v.physics[reached];
        const auto &parameters = v.parameters[reached];
        state.engine_output_scale = 0; // 40A1E3 before actual curve selection
        const auto selected = std::bit_cast<W>(state.torque_curve);
        if (selected >= parameters.torque_curves.size())
            throw SourceAllocationError(0x40a227, "cold engine curve pointer outside authored curve owner");
        sgn_source_config_engine_40a1d0(&parameters.rpm_max, &parameters.rpm_limit, &parameters.max_torque,
                                        parameters.torque_curves[selected].data(), state.engine_output_curve.data(),
                                        &state.engine_output_scale);
    } // 40A6C4: same x87 child already used by actual configuration loader
    id = current(w, 0x40a6c9);
    put<0>(v.physics[id].turbo_active, 0x00000000u);              // 40A6D1: +2250
    put<0>(v.parameters[id].turbo_acceleration, 0x41c80000u);     // 40A6D7: +0D60
    put<0>(v.parameters[id].turbo_decay, 0x3f19999au);            // 40A6E1: +0D68
    put<0>(v.parameters[id].turbo_max, 0x459c4000u);              // 40A6EB: +0D64
    put<0>(v.parameters[id].turbo_scale, 0x3f000000u);            // 40A6F5: +0D6C
    put<0>(v.physics[id].turbo_force, 0x3f800000u);               // 40A6FB: +2258
    put<0>(v.parameters[id].turbo_consumption, 0x3c23d70au);      // 40A701: +0F70
    put<0>(v.parameters[id].turbo_energy_threshold, 0x3e99999au); // 40A70B: +0F74
    sgn_cold_turbo_40a71b(v.parameters[id].turbo_curve.data());   // 128 FILD/FMUL/FSTP stores
    put<0>(v.parameters[id].fuel_capacity, 0x42960000u);          // 40A743: +0F78
    put<0>(v.physics[id].fuel, 0x42960000u);                      // 40A749: +2270
    put<0>(v.parameters[id].fuel_unit_mass, 0x3f800000u);         // 40A74F: +0F7C
    put<0>(v.parameters[id].fuel_consumption, 0x3ba3d70au);       // 40A755: +0F80
    put<0>(v.parameters[id].refuel_rate, 0x3f800000u);            // 40A75F: +1184
    for (auto &value : v.parameters[id].consumption_curve)
        value = 1;                                           // 40A76E128DWORD
    put<0>(v.parameters[id].consumption_curve, 0x00000000u); // 40A770: +0F84
    put<0>(v.parameters[id].launch_time, 0x41900000u);       // 40A772: +1B94
    put<0>(v.parameters[id].launch_tolerance, 0x40801062u);  // 40A77C: +1B98
    sgn_cold_launch_40a78b(&v.parameters[id].mass,
                           &v.parameters[id].launch_speed);            // 40A799 adds20 to sourceEDX before FSTP+1B7C
    put<0>(v.parameters[id].max_pitch_speed, 0x45ea6000u);             // 40A7A2: +1BA0
    put<0>(v.parameters[id].max_yaw_speed, 0x45ea6000u);               // 40A7A8: +1BA4
    put<0>(v.physics[id].contact_class, 0x00000000u);                  // 40A7AE: +1FD8
    put<0>(v.physics[id].contact_counter, 0x00000000u);                // 40A7B4: +71EC
    put<0>(v.physics[id].stunt.active, 0x00000000u);                   // 40A7BA: +71F0
    put<0>(v.physics[id].stunt.previous_contact_counter, 0x00000000u); // 40A7C0: +71F8
    put<0>(v.physics[id].airborne_ticks, 0x00000000u);                 // 40A7C6: +71FC
    put<0>(v.air[id].completed_airtime, 0x00000000u);                  // 40A7CC: +7200
    put<0>(v.physics[id].stunt.launch_tick, 0x00000000u);              // 40A7D2: +7234
    put<0>(v.physics[id].stunt.launch_height, 0x469c4000u);            // 40A7D8: +7238
    put<0>(v.contact[id].impact_count.value, 0x00000000u);             // 40A7E2: +2568
    put<0>(v.contact[id].impact_count.capacity, 0x000003e8u);          // 40A7ED: +256C
    put<0>(v.physics[id].energy_capacity, 0x447a0000u);                // 40A7F7: +2574
    put<0>(v.physics[id].energy, 0x447a0000u);                         // 40A7FD: +2570
    put<0>(v.load[id].current_refuel_rate, std::bit_cast<W>(v.parameters[id].refuel_rate)); // 40A809: +2274
    put<0>(v.load[id].sounds.engine_rev, 0x00000001u);                                      // 40A80F: +770C
    put<0>(v.load[id].sounds.engine_idle, 0x00000006u);                                     // 40A819: +7710
    put<0>(v.load[id].sounds.engine_boost, 0x0000000bu);                                    // 40A823: +7714
    sgn_cold_friction_40a82d(v.parameters[id].friction_curve.data());
}
void initialize_source_world_frame_globals_401de0(RaceSessionPlatform &platform)
{
    admit(platform, 0x401de0);
    initialize_world_frame_globals(platform.state().world_frame); // eight scalar stores then three44EE10 matrices
}
} // namespace sgn::retail
