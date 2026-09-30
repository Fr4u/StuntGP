#include "sgn/retail_cold_start.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::compile_time::as_float;
} // namespace
Curve cold_torque_curve()
{
    // 0040A160..0040A1C3. ST1 accumulates the old step before ST0 is adjusted.
    Curve result{};
    double running = 0;
    double step = as_float(0x3cb66666);
    for (unsigned i = 0; i < 128; ++i)
    {
        result[i] = vehicle_f32(std::sqrt(running));
        running += step;
        if (i > 32)
        {
            if (i < 64)
            {
                step -= 0.0009765625; // double at 0046E420
                if (step < 0)
                    step = as_float(0xbb23d70a);
            }
            if (i > 76)
                step -= as_float(0x399d4952);
        }
    }
    return result;
}
void initialize_cold_numeric_storage(ColdNumericStorage world, float wheel_load)
{
    if (!std::isfinite(wheel_load))
        throw std::invalid_argument("cold wheel load must be finite");
    // 0040A2F9..0040A316 clears eight original strides. Six of those are the
    // represented physical slots here; the allocation's two other strides are
    // deliberately not fabricated as extra physics vehicles.
    for (unsigned i = 0; i < 6; ++i)
    {
        world.parameters[i] = {};
        world.physics[i] = {};
        world.race[i] = {};
        world.contact[i] = {};
    }
    world.current_slot = 0;
    auto &p = world.parameters[0];
    auto &s = world.physics[0];
    auto &race = world.race[0];
    race.active = 1;
    s.orientation = s.incremental = s.secondary_rotation = vehicle_identity();
    // 0040A44C..0040A536, with literal single-precision constants.
    p.mass = as_float(0x447a0000);
    p.com_a = p.com_b = as_float(0x3f000000);
    p.com_height = as_float(0x41f00001);
    p.wheelbase = as_float(0x43740000);
    p.track_front = p.track_rear = p.width = as_float(0x43180000);
    p.length = as_float(0x43c80000);
    p.height = as_float(0x42e60000);
    p.steering_max = as_float(0x41c80000);
    p.steering_acceleration = 1;
    p.steering_curve.fill(1);
    p.brake_acceleration = as_float(0x40200000);
    p.brake_bias = as_float(0x3ecccccd);
    s.brake_signal = 255;
    p.drag_coefficient = as_float(0x3f19999a);
    p.lift_coefficient = as_float(0x3e99999a);
    p.frontal_area = as_float(0x3fee147b);
    s.air_factor = 1;
    // 0040A548..0040A5A4.
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &tyre = p.tyres[i];
        tyre.radius = as_float(0x3ea8f5c3);
        tyre.pressure = as_float(0x41f00000);
        tyre.static_friction = as_float(0x3f333333);
        tyre.kinetic_friction = as_float(0x3ecccccd);
        tyre.travel_in = tyre.travel_out = as_float(0x41200000);
        tyre.damping_in = tyre.damping_out = as_float(0x3f4ccccd);
        tyre.stiffness_in = tyre.stiffness_out = as_float(0x3ba3d70a);
        s.wheels[i].load = wheel_load;
    }
    // 0040A5A6..0040A681.
    p.gear_count = 6;
    p.shift_time = as_float(0x3f000000);
    s.gear = 1;
    s.clutch = 1;
    constexpr std::array<std::uint32_t, 8> ratio_bits = {0xc003d70a, 0,          0x40570a3d, 0x4003d70a,
                                                         0x3fb0a3d7, 0x3f851eb8, 0x3f6b851f, 0x3f4ccccd};
    for (unsigned i = 0; i < 8; ++i)
        p.ratios[i] = as_float(ratio_bits[i]);
    p.final_drive = as_float(0x4078f5c3);
    p.efficiency = as_float(0x3f666666);
    race.control_kind = 2;
    p.digital_steering.fill(1);
    p.digital_brake.fill(1);
    p.analog_steering.fill(1);
    p.analog_brake.fill(1);
    p.velocity_steering.fill(1);
    // 0040A683..0040A6C4, including the complete original curve builders.
    p.rpm_max = as_float(0x460ca000);
    p.rpm_limit = as_float(0x45dac000);
    p.rpm_idle = as_float(0x43160000);
    p.engine_decay = as_float(0x3f7851ec);
    p.max_torque = as_float(0x43c50000);
    s.engine_output_scale = as_float(0x43960000);
    p.torque_curves[0] = cold_torque_curve();
    build_engine_curve(p, s);
    // 0040A6C9..0040A770.
    p.turbo_acceleration = as_float(0x41c80000);
    p.turbo_decay = as_float(0x3f19999a);
    p.turbo_max = as_float(0x459c4000);
    p.turbo_scale = as_float(0x3f000000);
    s.turbo_force = 1;
    p.turbo_consumption = as_float(0x3c23d70a);
    p.turbo_energy_threshold = as_float(0x3e99999a);
    for (unsigned i = 0; i < 128; ++i)
        p.turbo_curve[i] = vehicle_f32(double(i) * 0.0078125);
    s.fuel = p.fuel_capacity = as_float(0x42960000);
    p.fuel_unit_mass = 1;
    p.fuel_consumption = as_float(0x3ba3d70a);
    p.refuel_rate = 1;
    p.consumption_curve.fill(1);
    p.consumption_curve[0] = 0;
    // 0040A772..0040A85B. EDX advances by 0x20 at 0040A799, so later
    // instruction displacements must include that adjustment.
    p.launch_time = as_float(0x41900000);
    p.launch_tolerance = as_float(0x40801062);
    p.launch_speed = vehicle_f32(double(p.mass) * 25);
    p.max_pitch_speed = p.max_yaw_speed = as_float(0x45ea6000);
    s.stunt.launch_height = as_float(0x469c4000);
    world.contact[0].impact_count.capacity = 1000;
    s.energy = s.energy_capacity = as_float(0x447a0000);
    for (unsigned i = 0; i < 128; ++i)
        p.friction_curve[i] = double(i) < as_float(0x424ccccd) ? 1 : as_float(0x3ecccccd);
}
} // namespace sgn::retail
