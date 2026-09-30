#include "sgn/vehicle.hpp"
#include "sgn/vehicle_dynamics_shared.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
// x87 intermediates are represented with binary64; explicit casts correspond
// to retail FST/FSTP binary32 stores. Oracle tests quantify residual differences.
float f32(double x)
{
    return static_cast<float>(x);
}
unsigned curve_index(double x)
{
    // Retail __ftol truncates then compares EAX unsigned. Accepted runtime
    // inputs are finite; negative indices therefore select the final sample.
    if (!std::isfinite(x))
        throw std::domain_error("non-finite retail curve index");
    if (x <= -1.0 || x >= 128.0)
        return 127;
    return static_cast<unsigned>(static_cast<int>(x));
}
Wheel &wheel_at(VehicleState &s, unsigned i)
{
    return s.wheels.at(i);
}
void apply_braking_force(const VehicleParameters &p, VehicleState &s, double force)
{
    // Shared instruction sequence 0040CD33..0040CE1A / 0040C3EC..0040C4D1.
    if (s.wheels[0].speed < 0)
        force = -force;
    const float first = f32(force * p.brake_bias);
    const double second = (1.0 - p.brake_bias) * force;
    s.wheels[0].acceleration = s.wheels[1].acceleration = first;
    s.wheels[2].acceleration = s.wheels[3].acceleration = f32(second);
    // The first threshold reloads an FSTP float; the second retains ST0.
    const float first_limit = f32(std::abs(double(first)) * 2);
    const float second_limit = f32(std::abs(second) * 2);
    for (unsigned i = 0; i < 4; ++i)
        if (std::abs(s.wheels[i].speed) < (i < 2 ? first_limit : second_limit))
            stop_wheel(s, i);
    for (unsigned i = 0; i < 4; ++i)
        wheel_braking_signal(s, i);
    brake_signal(s);
}
void steer_from_digital_input(const VehicleParameters &p, VehicleState &s, bool reverse)
{
    steering_boost(s);
    const bool increase = (reverse ? s.input.left : s.input.right) != 0;
    const bool decrease = (reverse ? s.input.right : s.input.left) != 0;
    if (increase || decrease)
    {
        const float amount =
            f32((double(p.digital_steering.at(static_cast<std::size_t>(s.steering_ramp))) * p.steering_acceleration) *
                (double(s.steering_boost) + 1));
        if (increase)
            vehicle_steer_increase(s.steering, p.steering_max, amount);
        else
            vehicle_steer_decrease(s.steering, p.steering_max, amount);
        ++s.steering_ramp;
    }
    else
        s.steering_ramp -= 6;
    s.steering_ramp = std::clamp(s.steering_ramp, 0, 127);
}
} // namespace

void digital_throttle(VehicleState &s)
{
    // 0040CB90. The uint32 airborne timer is zero-extended before FILD.
    if (s.contact_counter != 0)
        s.throttle = f32(double(s.throttle) * double(0.9f));
    s.control_flags[0] = s.control_flags[2];
    s.control_flags[2] = 0;
    s.turbo_active = 0;
    if (s.contact_counter == 0 && s.airborne_ticks > 60)
        return;
    if ((s.gear == 0 ? s.input.brake : s.input.throttle) != 0)
    {
        const double next = double(s.throttle) + double(0.15f);
        s.throttle = f32(next);
        if (next > 1)
            s.throttle = 1;
        if (s.input.turbo != 0)
            s.turbo_active = 1;
        s.control_flags[2] = 1;
    }
    else if (s.contact_counter != 0)
        s.throttle = f32(double(s.throttle) * 0.5);
}
void analog_throttle(VehicleState &s)
{
    // 0040C220. Unlike digital input, the current flag is set before the air guard.
    s.control_flags[0] = s.control_flags[2];
    s.control_flags[2] = 0;
    s.turbo_active = 0;
    const auto input = s.gear == 0 ? s.input.brake : s.input.throttle;
    if (input != 0)
    {
        s.control_flags[2] = 1;
        if (s.contact_counter == 0 && s.airborne_ticks > 60)
        {
            s.throttle = f32(double(s.throttle) * double(0.9f));
            return;
        }
        s.throttle = f32(double(input) * double(0.003921568859368563f));
        if (s.input.throttle != 0 && s.input.turbo != 0)
            s.turbo_active = 1;
    }
    else
        s.throttle = f32(double(s.throttle) * 0.5);
}
void wheel_braking_signal(VehicleState &s, unsigned wheel)
{
    // 00414AC0; comparison after FST still sees the unrounded ST0 value.
    auto &w = wheel_at(s, wheel);
    const double added = std::min(std::abs((double(w.acceleration) * w.acceleration) * w.speed) * double(0.1f), 60.0);
    const double value = added + w.braking_signal;
    w.braking_signal = f32(value);
    if (value >= 255)
        w.braking_signal = 255;
}
void brake_signal(VehicleState &s) noexcept
{
    s.brake_signal = 255;
}
void digital_brake(const VehicleParameters &p, VehicleState &s)
{
    // 0040CC90; the analog function has no equivalent contact early return.
    if (s.contact_counter == 0)
        return;
    s.control_flags[1] = s.control_flags[3];
    s.control_flags[3] = 0;
    if ((s.gear == 0 ? s.input.throttle : s.input.brake) != 0)
    {
        const double force =
            -(double(p.digital_brake.at(static_cast<std::size_t>(s.brake_ramp))) * p.brake_acceleration);
        s.control_flags[3] = 1;
        s.brake_ramp += 6;
        if (s.input.rear_lock != 0)
        {
            s.wheels[0].speed = s.wheels[1].speed = 0;
            brake_signal(s);
        }
        else
            apply_braking_force(p, s, force);
    }
    else
        s.brake_ramp = std::max(s.brake_ramp - 40, 0);
    if (s.brake_ramp > 127)
        s.brake_ramp = 127;
}
void analog_brake(const VehicleParameters &p, VehicleState &s)
{
    // 0040C320. __ftol truncates the negative byte-to-index product.
    s.control_flags[1] = s.control_flags[3];
    s.control_flags[3] = 0;
    const auto input = s.gear == 0 ? s.input.throttle : s.input.brake;
    if (input == 0)
        return;
    s.control_flags[3] = 1;
    if (s.input.rear_lock != 0)
    {
        s.wheels[0].speed = s.wheels[1].speed = 0;
        brake_signal(s);
        return;
    }
    const auto index = -static_cast<int>((double(input) * double(0.003921568859368563f)) * (-127.0));
    apply_braking_force(p, s, -(double(p.analog_brake.at(static_cast<std::size_t>(index))) * p.brake_acceleration));
}
void steering_boost(VehicleState &s)
{
    // 0040CE60 stores and reloads the float before both clamps.
    s.steering_boost = f32(s.input.steering_boost != 0 ? double(s.steering_boost) + double(0.1f)
                                                       : double(s.steering_boost) - double(0.4285714328289032f));
    s.steering_boost = std::clamp(s.steering_boost, 0.0f, 3.0f);
}
void digital_steering(const VehicleParameters &p, VehicleState &s)
{
    steer_from_digital_input(p, s, false);
}
void reversed_digital_steering(const VehicleParameters &p, VehicleState &s)
{
    steer_from_digital_input(p, s, true);
}
void digital_manual_shift(const VehicleParameters &p, VehicleState &s)
{
    if (p.manual_gears == 1 && s.input.shift != 0 && s.previous_shift == 0)
    {
        if (s.throttle > 0.5f)
            shift_up(p, s);
        else
            shift_down(p, s);
    }
}
void analog_manual_shift(const VehicleParameters &p, VehicleState &s)
{
    if (p.manual_gears != 0 && s.input.shift != 0 && s.previous_shift == 0)
    {
        if (s.throttle >= 0.5f)
            shift_up(p, s);
        else
            shift_down(p, s);
    }
}
void reset_camera_dependent_effects(VehicleState &s) noexcept
{
    // 004128F0 -> 00414D80, 00414C50, 00414EF0, 004152C0.
    s.camera_effects.values.fill(0);
}
void prepare_physical_parameters(const VehicleParameters &p, VehicleState &s)
{
    // 00418580; the override COM is the source's literal 0x41F00001.
    s.effective_com_height = p.com_height;
    s.effective_mass = f32(double(s.fuel) * p.fuel_unit_mass + p.mass);
    if (s.mass_override == 1)
    {
        s.effective_mass = 1000;
        s.effective_com_height = std::bit_cast<float>(0x41f00001u);
    }
    if (s.special_mode == 1 && s.effective_com_height > 10)
        s.effective_com_height = 10;
    const double friction = s.gear == 0 ? 2.0 : s.performance_multiplier;
    for (unsigned i = 0; i < 4; ++i)
    {
        s.wheels[i].static_friction = f32(friction * p.tyres[i].static_friction);
        s.wheels[i].kinetic_friction = f32(friction * p.tyres[i].kinetic_friction);
    }
    s.torque = f32(double(s.performance_multiplier) * s.torque);
}
void update_support_basis(VehicleState &s, const VehicleSupport &support)
{
    // 00414710 keeps the X averages in x87 registers, while Y/Z are stored
    // twice. Column W lanes are preserved by the original normalizer.
    const auto &a = support.current[0];
    const auto &b = support.current[1];
    const auto &c = support.current[2];
    const auto &d = support.current[3];
    auto half_sum = [](float x, float y) { return f32(double(f32(double(x) + y)) * 0.5); };
    s.basis_x = vehicle_normalize({f32((double(a.x) + c.x) * 0.5 - (double(b.x) + d.x) * 0.5),
                                   f32(double(half_sum(a.y, c.y)) - half_sum(b.y, d.y)),
                                   f32(double(half_sum(c.z, a.z)) - half_sum(d.z, b.z)), s.basis_x.w});
    s.basis_z = vehicle_normalize({f32((double(d.x) + c.x) * 0.5 - (double(b.x) + a.x) * 0.5),
                                   f32(double(half_sum(d.y, c.y)) - half_sum(b.y, a.y)),
                                   f32(double(half_sum(d.z, c.z)) - half_sum(b.z, a.z)), s.basis_z.w});
    const auto up =
        vehicle_normalize({f32(double(s.basis_z.z) * s.basis_x.y - double(s.basis_z.y) * s.basis_x.z),
                           f32(double(s.basis_z.x) * s.basis_x.z - double(s.basis_z.z) * s.basis_x.x),
                           f32(double(s.basis_x.x) * s.basis_z.y - double(s.basis_z.x) * s.basis_x.y), s.basis_y_w});
    s.up = {up.x, up.y, up.z};
}
void approach_body_speed(VehicleState &s)
{
    // 00406270.
    if (s.contact_class != 0)
        for (auto &w : s.wheels)
            w.speed = f32((double(s.speed) - w.speed) * 0.5 + w.speed);
}
void damp_auxiliary_motion(VehicleState &s)
{
    // 00413330 repeats four substeps on the same pair of vectors. It does not
    // increment the source pointer to visit four independent wheel states.
    for (unsigned step = 0; step < 4; ++step)
    {
        const double x = double(s.auxiliary_displacement.x) * double(0.03333333507180214f);
        const float y = f32(double(s.auxiliary_displacement.y) * double(0.03333333507180214f));
        const float z = f32(double(s.auxiliary_displacement.z) * double(0.03333333507180214f));
        auto &velocity = s.auxiliary_velocity;
        auto &displacement = s.auxiliary_displacement;
        velocity.x = f32(double(velocity.x) - x);
        velocity.y = f32(double(velocity.y) - y);
        velocity.z = f32(double(velocity.z) - z);
        displacement.x = f32(double(velocity.x) + displacement.x);
        displacement.y = f32(double(velocity.y) + displacement.y);
        displacement.z = f32(double(velocity.z) + displacement.z);
        velocity.x = f32(double(velocity.x) * double(0.9f));
        velocity.y = f32(double(velocity.y) * double(0.9f));
        velocity.z = f32(double(velocity.z) * double(0.9f));
    }
}

void distribute_drive(const VehicleParameters &p, VehicleState &s, float acceleration)
{
    // 0040e3a0: the first axle is wheel 0/1, second axle wheel 2/3.
    double value = acceleration;
    if (acceleration > 0 && s.turbo_rpm != 0 && s.contact_class == 0)
        value = value * s.turbo_force + value;
    value *= 4;
    if (p.drive_mode == 1 || p.drive_mode == 2)
    {
        const double second = p.drive_mode == 1 ? value : value * p.power_split;
        for (unsigned i : {2u, 3u})
            s.wheels[i].acceleration = f32(second + s.wheels[i].acceleration);
    }
    if (p.drive_mode == 0 || p.drive_mode == 2)
    {
        const double first = p.drive_mode == 0 ? value : (1.0 - p.power_split) * value;
        for (unsigned i : {0u, 1u})
            s.wheels[i].acceleration = f32(first + s.wheels[i].acceleration);
    }
}
void engage_clutch(const VehicleParameters &p, VehicleState &s)
{
    if (s.clutch < 1)
    {
        const double next = 1.0 / (double(p.shift_time) * 60) + s.clutch;
        s.clutch = f32(next);
        if (next >= 1)
            s.clutch = 1;
        if (s.clutch < 0.5f && s.throttle > 0)
            s.throttle = 0;
    }
}
double driven_mass(const VehicleParameters &p, const VehicleState &s, float mass)
{
    const auto &w = s.wheels;
    bool unloaded =
        (p.drive_mode == 0 && w[0].contact_countdown + w[1].contact_countdown < 8) ||
        (p.drive_mode == 1 && w[2].contact_countdown + w[3].contact_countdown < 8) ||
        (p.drive_mode == 2 &&
         w[0].contact_countdown + w[1].contact_countdown + w[2].contact_countdown + w[3].contact_countdown < 16);
    return unloaded ? double(mass) * 0.25 : mass;
}
void update_turbo(const VehicleParameters &p, VehicleState &s)
{
    if (s.energy < double(s.energy_capacity) * p.turbo_energy_threshold)
        s.turbo_active = 0;
    if (s.turbo_active == 1)
    {
        const double next = double(p.turbo_acceleration) + s.turbo_rpm;
        s.turbo_rpm = f32(next);
        if (next >= p.turbo_max)
            s.turbo_rpm = f32(double(p.turbo_max) - 1);
    }
    else
    {
        const double next = double(p.turbo_decay) * s.turbo_rpm;
        s.turbo_rpm = f32(next);
        if (next < 50)
        {
            s.turbo_rpm = 0;
            s.turbo_force = 0;
            return;
        }
    }
    s.turbo_force = f32(double(p.turbo_curve[curve_index(double(s.turbo_rpm) * 128 / p.turbo_max)]) * p.turbo_scale);
}
void limit_reverse_rpm(const VehicleParameters &p, VehicleState &s)
{
    if (s.gear == 0)
    {
        s.turbo_active = 0;
        double rpm = s.rpm;
        while (double(p.rpm_limit) * 0.5 < rpm)
        {
            rpm = double(s.rpm) * double(0.8f);
            s.rpm = f32(rpm);
        }
    }
}
float limit_low_energy(const VehicleState &s, float acceleration)
{
    if (s.special_mode != 1 && s.energy <= 10)
    {
        double speed = double(s.speed) * double(0.013333334f);
        if (speed > 1)
            speed = 1;
        return f32((1 - speed * speed) * acceleration);
    }
    return acceleration;
}
void update_reference_load(VehicleState &s, float gravity)
{
    const float load = f32(double(gravity) * s.effective_mass * 0.25);
    for (auto &w : s.wheels)
        w.reference_load = load;
}
void rolling_resistance(const VehicleParameters &p, VehicleState &s, unsigned index, float longitudinal_speed,
                        float &axle_force)
{
    auto &w = wheel_at(s, index);
    const double speed = std::abs(double(longitudinal_speed)) * double(1.3424487f);
    double force =
        -((speed * speed * double(0.000035f) + double(0.15f)) * (1.0 / p.tyres.at(index).pressure) + double(0.005f));
    force = force * w.load / (double(s.effective_mass) * 0.25) * w.surface_rolling * 100 * double(0.00027777778f);
    if (std::abs(double(longitudinal_speed)) < std::abs(force))
        force = -std::abs(double(longitudinal_speed));
    if (w.speed < 0)
        force = -force;
    w.rolling_force = f32(force);
    w.speed = f32(force + w.speed);
    axle_force = f32(force * 0.5 + axle_force);
}
namespace
{
void aerodynamic_drag_retained(const VehicleParameters &p, VehicleState &s, float &front, float &rear,
                               std::optional<double> special_drag)
{
    const double speed = double(s.speed) * double(0.6f);
    s.drag_force = f32(speed * speed * s.air_factor * p.frontal_area * p.drag_coefficient * double(0.6128368f));
    // 0040F4D8 stores the base force. 00405860 retains its product until
    // 00405874's store; the disabled branch makes no second addition.
    if (special_drag)
        s.drag_force = f32(*special_drag + double(s.drag_force));
    const double force = double(s.drag_force) / s.effective_mass * 100 * double(0.00027777778f);
    s.drive_force = f32(double(s.drive_force) - force);
    front = f32(double(front) - force);
    rear = f32(double(rear) - force);
    for (auto &w : s.wheels)
        w.speed = f32(double(w.speed) - force);
}
} // namespace
void aerodynamic_drag(const VehicleParameters &p, VehicleState &s, float &front, float &rear, float special_drag)
{
    aerodynamic_drag_retained(p, s, front, rear, double(special_drag));
}
void attenuate_steering(const VehicleParameters &p, VehicleState &s)
{
    s.steering = f32(double(p.steering_curve[curve_index(std::abs(double(s.speed)))]) * s.steering);
}
void tyre_friction(const VehicleParameters &p, VehicleState &s, unsigned index, float &lateral, float &longitudinal)
{
    auto &w = wheel_at(s, index);
    const float load = f32(double(w.load) / (double(s.effective_mass) * 0.25));
    const float cs = f32(double(w.static_friction) * w.surface_grip);
    const float ck = f32(double(w.kinetic_friction) * w.surface_grip);
    const float original_lateral = lateral, original_longitudinal = longitudinal;
    const double magnitude =
        std::sqrt(double(lateral) * lateral + double(longitudinal) * longitudinal * double(0.64000005f));
    const float stored_magnitude = f32(magnitude);
    double available = double(load) * p.friction_curve[curve_index(magnitude)];
    double threshold = available * cs;
    if (std::abs(threshold) < double(0.01f))
        threshold = double(0.01f);
    w.grip_usage = f32(std::abs(double(stored_magnitude) / threshold));
    if (stored_magnitude <= threshold)
    {
        lateral = 0;
        return;
    }
    const double lateral_change = std::min(std::abs(double(original_lateral)), available) * ck;
    lateral = f32(original_lateral < 0 ? original_lateral + lateral_change : original_lateral - lateral_change);
    if (original_longitudinal > 0)
        available *= 3;
    const float longitudinal_change = f32(std::min(std::abs(double(original_longitudinal)), available));
    longitudinal = f32(double(longitudinal_change) * ck * (original_longitudinal < 0 ? -1.0 : 1.0));
}
void average_grip(VehicleState &s)
{
    double total = 0;
    for (const auto &w : s.wheels)
        total += w.grip_usage;
    s.average_grip = f32(total * 0.25);
}
void airborne_wheel_decay(VehicleState &s)
{
    if (s.contact_class == 1)
        for (auto &w : s.wheels)
            w.speed = f32(double(w.speed) * double(0.995f));
}
void classify_contacts(VehicleState &s)
{
    unsigned count = 0;
    for (const auto &w : s.wheels)
        if (w.contact_countdown < 4)
            ++count;
    if (s.contact_class == 0)
        s.contact_hold = 16;
    else if (s.contact_hold != 0)
        --s.contact_hold;
    s.previous_contact_class = s.contact_class;
    s.contact_class = count == 0 ? 0 : (count == 4 ? 1 : 2);
}
void aerodynamic_lift(const VehicleParameters &p, VehicleState &s)
{
    if (s.contact_class == 0)
    {
        const double speed = double(s.speed) * double(0.6f);
        const double lift = speed * speed * s.air_factor * p.frontal_area * p.lift_coefficient * double(-0.615f);
        s.lift_force = f32(lift);
        const double force = lift / (double(s.effective_mass) * 4);
        s.velocity = {f32(double(f32(force * s.up.x)) + s.velocity.x), f32(double(f32(force * s.up.y)) + s.velocity.y),
                      f32(force * s.up.z + s.velocity.z)};
    }
}
void clear_wheel_acceleration(VehicleState &s)
{
    for (auto &w : s.wheels)
        w.acceleration = 0;
}
void stop_wheel(VehicleState &s, unsigned index)
{
    auto &w = wheel_at(s, index);
    w.acceleration = 0;
    w.speed = 0;
}
void build_engine_curve(const VehicleParameters &p, VehicleState &s)
{
    s.engine_output_scale = 0;
    const auto &curve = p.torque_curves.at(static_cast<unsigned>(s.torque_curve));
    for (unsigned j = 0; j < 128; ++j)
    {
        const double rpm = double(j) * p.rpm_max * 0.0078125;
        const float value = f32(rpm * double(0.10471976f) * curve[j] * double(0.0013404826f));
        s.engine_output_curve[j] = value;
        if (rpm <= p.rpm_limit && value > s.engine_output_scale)
            s.engine_output_scale = value;
    }
    s.engine_output_scale = f32(double(p.max_torque) * s.engine_output_scale);
}
void shift_up(const VehicleParameters &p, VehicleState &s)
{
    if (static_cast<unsigned>(s.gear) < static_cast<unsigned>(p.gear_count))
    {
        ++s.gear;
        s.gear_changed = 1;
        s.clutch = 0;
    }
}
void shift_down(const VehicleParameters &p, VehicleState &s)
{
    if (s.gear != 0)
    {
        s.downshift_ticks = static_cast<std::int32_t>(double(p.shift_time) * 60);
        --s.gear;
        s.gear_changed = 1;
        s.clutch = 0;
    }
}
void shift_neutral(VehicleState &s)
{
    if (s.gear != 1)
    {
        s.gear = 1;
        s.gear_changed = 1;
        s.clutch = 0;
    }
}
void automatic_shift(const VehicleParameters &p, VehicleState &s, const EngineContext &context)
{
    if (!context.automatic_shift_allowed || p.manual_gears != 0)
        return;
    if (s.rpm < double(p.rpm_idle) + 200 &&
        std::all_of(
            s.control_flags.begin(), s.control_flags.end(), [](auto x) { return x == 0; }) &&
        s.gear != 1 && s.input.shift == 0)
        shift_neutral(s);
    if (s.downshift_ticks != 0)
    {
        --s.downshift_ticks;
        s.rpm = f32(double(s.rpm) + 200);
    }
    if (s.clutch != 1)
        return;
    if (s.throttle < double(0.4f))
    {
        // 0040e6a7..0040e6b8 compares signed speed; later FABS applies to a boolean.
        if ((s.gear == 1 || s.gear == 2) && s.control_flags[3] == 1 && s.speed < double(8.333333f))
            shift_down(p, s);
        if (static_cast<unsigned>(s.gear) > 2 && s.rpm < double(p.rpm_limit) * double(0.4f))
            shift_down(p, s);
    }
    else
    {
        bool up = false;
        if (s.gear == 1)
            up = double(p.rpm_limit) * 0.5 < s.rpm;
        else
        {
            if (s.gear == 0)
                return;
            // 0040e4f0: previous gear ratio, wheel 2 speed, no guessed gearing.
            if (s.gear > 2)
            {
                const double previous_rpm = double(s.wheels[2].speed) * p.ratios.at(static_cast<unsigned>(s.gear - 1)) *
                                            p.final_drive * 3600 / (double(p.tyres[0].radius) * double(6.2831855f));
                if (previous_rpm < double(p.rpm_limit) * double(0.9f) - 2500)
                    shift_down(p, s);
            }
            if (s.gear > 6)
                return;
            up = double(p.rpm_limit) * double(0.9f) < s.rpm;
        }
        if (up)
            shift_up(p, s);
    }
}
void consume_engine_energy(const VehicleParameters &p, VehicleState &s, const EngineContext &context, float rpm)
{
    float consumption =
        f32(double(p.consumption_curve[curve_index(double(rpm) * 128 / p.rpm_max)]) * p.fuel_consumption);
    if (s.upgrade_active == 1)
        consumption = f32(((1.0 - s.upgrade_condition) * context.engine_consumption_modifier + 1) * consumption);
    s.fuel = f32(double(s.fuel) - consumption);
    const double energy = double(s.energy) - double(consumption) * s.energy_capacity / p.fuel_capacity;
    s.energy = energy < 0 ? 0 : f32(energy);
}
void consume_turbo_energy(const VehicleParameters &p, VehicleState &s, const EngineContext &context, float rpm)
{
    float consumption =
        f32(double(p.consumption_curve[curve_index(double(rpm) * 128 / p.turbo_max)]) * p.turbo_consumption);
    if (s.upgrade_active == 1)
        consumption = f32(((1.0 - s.upgrade_condition) * context.turbo_consumption_modifier + 1) * consumption);
    const double energy = double(s.energy) - double(consumption) * s.energy_capacity / p.fuel_capacity;
    s.energy = energy < 0 ? 0 : f32(energy);
}
void engine_tick(VehicleParameters &p, VehicleState &s, const EngineContext &context)
{
    p.ratios[1] = 2;
    limit_reverse_rpm(p, s);
    if (context.game_mode != 3 && s.energy_exempt != 1)
    {
        consume_engine_energy(p, s, context, s.rpm);
        consume_turbo_energy(p, s, context, s.turbo_rpm);
    }
    update_turbo(p, s);
    automatic_shift(p, s, context);
    engage_clutch(p, s);
    const float radius = f32(double(p.tyres[0].radius) * double(0.01f));
    float clutch = s.clutch;
    const unsigned gear = static_cast<unsigned>(s.gear);
    if (s.throttle < double(0.1f))
        s.rpm = f32(double(p.engine_decay) * s.rpm);
    if (gear == 1)
        clutch = 0;
    const float mass = f32(driven_mass(p, s, f32(double(clutch) * s.effective_mass)) + 50);
    const float ratio = p.ratios.at(gear);
    float acceleration = f32(double(ratio) * s.torque * p.final_drive * p.efficiency * s.throttle / mass * 100 *
                             double(0.00027777778f) * 3);
    acceleration = limit_low_energy(s, acceleration);
    const float circumference = f32(double(radius) * double(6.2831855f));
    s.rpm = f32(double(ratio) * p.final_drive * acceleration * 60 / circumference + s.rpm);
    distribute_drive(p, s, f32(double(acceleration) * clutch));
    auto limit = [&] {
        double rpm = s.rpm;
        while (rpm > p.rpm_limit)
        {
            rpm = double(s.rpm) * double(0.9f);
            s.rpm = f32(rpm);
        }
    };
    limit();
    if (s.rpm < 100)
        s.rpm = 100;
    if (s.rpm < p.rpm_idle)
        s.rpm = f32(double(s.rpm) * double(1.05f));
    if (gear != 1)
    {
        const double wheel_speed =
            p.drive_mode == 1
                ? s.wheels[2].speed
                : (p.drive_mode == 0 ? s.wheels[0].speed : (double(s.wheels[2].speed) + s.wheels[0].speed) * 0.5);
        const double mismatch = (double(radius) * s.rpm * double(6.2831855f) / (double(ratio) * p.final_drive * 60) -
                                 wheel_speed * double(0.6f)) *
                                clutch;
        const float change = f32(double(mass) * double(0.02f) * ratio * mismatch * p.final_drive / circumference);
        s.rpm = f32(double(s.rpm) - change);
        distribute_drive(p, s, f32((50.0 / mass) * mismatch * double(1.6666666f)));
    }
    limit();
    const auto index = curve_index(double(s.rpm) * 128 / p.rpm_limit);
    s.engine_output = f32(double(s.engine_output_curve[index]) * s.engine_output_scale);
    const double torque = double(p.torque_curves.at(static_cast<unsigned>(s.torque_curve))[index]) * p.max_torque;
    s.torque = torque < 250 ? 250 : f32(torque);
}
void suspension_pose(const VehicleParameters &p, VehicleState &s)
{
    const auto &w = s.suspension;
    const double roll = ((double(w[0]) + w[2]) * 0.5 - (double(w[3]) + w[1]) * 0.5) *
                        (1.0 / ((double(p.track_rear) + p.track_front) * 0.5));
    s.body_roll = f32((roll + s.body_roll) * 0.5);
    double pitch = (-1.0 / p.wheelbase) * ((double(w[0]) + w[1]) * 0.5 - (double(w[3]) + w[2]) * 0.5);
    if (s.turbo_active == 1 && std::abs(double(s.speed)) < 50 && pitch < 0)
        pitch = ((double(s.turbo_rpm) * 16 / p.turbo_max) / (double(s.speed) * double(0.1f) + 1)) * pitch;
    const double next = (pitch * 0.125 + s.body_pitch) * double(0.95f);
    s.body_pitch = next > double(0.3f) ? 0.3f : (next < double(-0.3f) ? -0.3f : f32(next));
    s.body_heave = 0;
    for (auto v : w)
        s.body_heave = f32(double(v) + s.body_heave);
    s.body_heave = f32(double(s.body_heave) * -0.25 + p.ride_height);
}
void suspension_tick(const VehicleParameters &p, VehicleState &s, std::uint32_t tick, VehicleRandom &random)
{
    float load_sum = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto &w = s.wheels[i];
        const auto &t = p.tyres[i];
        double force = double(w.load) - w.reference_load;
        load_sum = f32(double(load_sum) + w.load);
        if (w.surface_roughness != 0 && (tick & 3) == 0)
        {
            const double noise = random.signed_value(f32(double(w.surface_roughness) + w.surface_roughness));
            force = (noise - w.surface_roughness) * s.speed * double(0.0054f) * 3 + f32(force);
        }
        const bool out = s.suspension[i] < 0;
        s.suspension[i] = f32(double(out ? t.damping_out : t.damping_in) * s.suspension[i] +
                              force * (out ? t.stiffness_out : t.stiffness_in));
        if (s.suspension[i] > t.travel_in)
            s.suspension[i] = t.travel_in;
        else if (s.suspension[i] < -double(t.travel_out))
            s.suspension[i] = -t.travel_out;
    }
    s.average_load = f32(double(load_sum) * 0.25);
    suspension_pose(p, s);
}
void acceleration_tilt(const VehicleParameters &p, VehicleState &s)
{
    const float pitch = f32(((double(s.basis_z.z) * s.acceleration.z + double(s.basis_z.y) * s.acceleration.y) +
                             double(s.basis_z.x) * s.acceleration.x) *
                            s.effective_com_height / p.wheelbase);
    const float roll = f32(((double(s.basis_x.z) * s.acceleration.z + double(s.basis_x.y) * s.acceleration.y) +
                            double(s.basis_x.x) * s.acceleration.x) *
                           s.effective_com_height / ((double(p.track_rear) + p.track_front) * 0.5));
    auto change = vehicle_transpose(s.orientation);
    vehicle_rotate(change, 0, -pitch);
    vehicle_rotate(change, 2, roll);
    vehicle_premultiply(change, s.orientation);
    vehicle_premultiply(s.incremental, change);
}
void support_rotation(VehicleVector body, VehicleVector point, float angle, VehicleMatrix &incremental)
{
    auto z = vehicle_normalize(
        {f32(double(body.x) - point.x), f32(double(body.y) - point.y), f32(double(body.z) - point.z), 0});
    auto x = vehicle_normalize({f32(double(z.z) - 0.0), 0, f32(0.0 - double(z.x)), 0});
    auto y = vehicle_normalize(vehicle_cross(x, z));
    auto basis = vehicle_basis(x, y, z), change = vehicle_transpose(basis);
    vehicle_rotate(change, 1, f32(double(angle) * double(57.295776f)));
    auto result = vehicle_identity();
    vehicle_multiply_basis(result, change, basis);
    vehicle_orthogonalize(result);
    vehicle_premultiply(incremental, result);
}
void wheel_rotation(VehicleState &s, const VehicleSupport &support, unsigned index, float inverse_gravity)
{
    if (index >= 4)
        throw std::out_of_range("vehicle wheel index");
    const auto local = support.local[index];
    const auto &m = s.incremental;
    const auto &w = s.wheels[index];
    const float radius =
        f32(std::sqrt((double(local.z) * local.z + double(local.y) * local.y) + double(local.x) * local.x));
    const float mass = f32(double(inverse_gravity) * w.reference_load);
    const float load = w.load > w.reference_load ? w.reference_load : w.load;
    const float tx = f32((double(local.z) * m[8] + double(local.y) * m[4]) + double(local.x) * m[0]);
    const float ty = f32((double(local.z) * m[9] + double(local.y) * m[5]) + double(local.x) * m[1]);
    const float tz = f32((double(local.z) * m[10] + double(local.y) * m[6]) + double(local.x) * m[2]);
    const double dx = double(tx) - local.x;
    const float dy = f32(double(ty) - local.y), dz = f32(double(tz) - local.z);
    // The original deliberately reads wheel zero's normal, even for indices 1..3.
    const auto normal = s.wheels[0].contact_normal;
    const double projection = (double(dy) * normal.y + double(dz) * normal.z) + dx * normal.x;
    const float projection_store = f32(projection);
    const VehicleVector tangent = {f32(dx - projection * normal.x),
                                   f32(double(dy) - f32(double(projection_store) * normal.y)),
                                   f32(double(dz) - f32(double(projection_store) * normal.z)), 0};
    float length{};
    const auto direction = vehicle_normalize(tangent, &length);
    double force = double(length) * mass;
    if (force > double(load) * w.static_friction)
        force = double(load) * w.kinetic_friction * 0.5;
    float angle = f32(force / (double(mass) * radius));
    auto side = vehicle_normalize(vehicle_cross({s.up.x, s.up.y, s.up.z, 0}, local));
    if (s.up.y < 0)
        angle = -angle;
    if ((double(direction.z) * side.z + double(direction.y) * side.y) + double(direction.x) * side.x >= 0)
        angle = -angle;
    support_rotation(support.body_position, support.current[index], angle, s.incremental);
}
void bank_displacement(const VehicleParameters &p, VehicleState &s, Vec3 gravity)
{
    if (s.contact_class != 0)
        return;
    double lateral =
        (double(gravity.x) * s.basis_x.x + double(gravity.y) * s.basis_x.y) + double(gravity.z) * s.basis_x.z;
    if (std::abs(lateral) < 0.2079116)
        return;
    if (s.speed < 1)
        lateral = 0;
    const double force = (double(s.effective_mass) * p.com_a) * lateral * double(0.003125f);
    const float x = f32(force * s.basis_x.x), y = f32(force * s.basis_x.y);
    s.front_axle_position.x = f32(double(x) + s.front_axle_position.x);
    s.front_axle_position.y = f32(double(y) + s.front_axle_position.y);
    s.front_axle_position.z = f32(force * s.basis_x.z + s.front_axle_position.z);
}
void apply_gravity(Vec3 &velocity, Vec3 gravity, float scale)
{
    if ((double(velocity.x) * velocity.x + double(velocity.y) * velocity.y) + double(velocity.z) * velocity.z >
        double(27777.777f))
        return;
    const double x = double(gravity.x) * scale;
    const float y = f32(double(gravity.y) * scale), z = f32(double(gravity.z) * scale);
    velocity.x = f32(x + velocity.x);
    velocity.y = f32(double(y) + velocity.y);
    velocity.z = f32(double(z) + velocity.z);
}
void vehicle_dynamics_core(VehicleParameters &p, VehicleState &s, const VehicleSupport &support,
                           const VehicleDynamicsContext &context, VehicleRandom &random)
{
    VehicleDynamicsWorkspace workspace;
    vehicle_dynamics_core(p, s, support, context, random, workspace);
}
void vehicle_dynamics_core(VehicleParameters &p, VehicleState &s, const VehicleSupport &support,
                           const VehicleDynamicsContext &context, VehicleRandom &random,
                           VehicleDynamicsWorkspace &workspace)
{
    vehicle_dynamics_core(p, s, support, context, random, workspace, double(context.special_drag));
}
void vehicle_dynamics_core(VehicleParameters &p, VehicleState &s, const VehicleSupport &support,
                           const VehicleDynamicsContext &context, VehicleRandom &random,
                           VehicleDynamicsWorkspace &workspace, std::optional<double> source_drag_adjustment)
{
    engine_tick(p, s, context.engine);
    update_reference_load(s, context.gravity_magnitude);
    classify_contacts(s);
    for (auto &w : s.wheels)
        w.speed = std::clamp(f32(double(w.acceleration) + w.speed), -416.66666f, 416.66666f);
    // The four original abs(speed)<0 tests never fire for finite accepted input.
    s.drive_force = 0;
    workspace.steering = vehicle_rotation(1, s.steering);
    const auto &steering = workspace.steering;
    auto right = s.basis_x, forward = s.basis_z;
    const VehicleVector up{s.up.x, s.up.y, s.up.z, s.basis_y_w};
    const double steered_fx =
        (double(steering[8]) * forward.z + double(steering[4]) * forward.y) + double(steering[0]) * forward.x;
    const double steered_fy =
        (double(steering[9]) * forward.z + double(steering[5]) * forward.y) + double(steering[1]) * forward.x;
    const double steered_fz =
        (double(steering[10]) * forward.z + double(steering[6]) * forward.y) + double(steering[2]) * forward.x;
    const VehicleVector steered_right{
        f32((double(steering[8]) * right.z + double(steering[4]) * right.y) + double(steering[0]) * right.x),
        f32((double(steering[9]) * right.z + double(steering[5]) * right.y) + double(steering[1]) * right.x),
        f32((double(steering[10]) * right.z + double(steering[6]) * right.y) + double(steering[2]) * right.x), 0};
    auto axle = [&](VehicleVector &a, double half) {
        a.x = f32(double(f32(half * forward.x)) + s.position.x);
        a.y = f32(double(f32(half * forward.y)) + s.position.y);
        a.z = f32(double(f32(half * forward.z)) + s.position.z);
    };
    axle(s.front_axle_position, double(p.wheelbase) * 0.5);
    axle(s.rear_axle_position, double(p.wheelbase) * -0.5);
    const double side_speed = (double(steered_right.x) * s.velocity.x + double(steered_right.y) * s.velocity.y) +
                              double(steered_right.z) * s.velocity.z;
    const float projected_x = f32(side_speed * steered_right.x), projected_y = f32(side_speed * steered_right.y);
    float front_force = f32(std::clamp(-((side_speed * steered_right.z * forward.z + double(projected_y) * forward.y) +
                                         double(projected_x) * forward.x),
                                       -0.5, 0.5));
    float rear_force = 0;
    const float speed =
        f32((double(forward.x) * s.velocity.x + double(forward.y) * s.velocity.y) + double(forward.z) * s.velocity.z);
    const float front_speed = f32((steered_fx * s.velocity.x + steered_fy * s.velocity.y) + steered_fz * s.velocity.z);
    const double transfer =
        -((double(speed) * forward.z * steered_right.z + double(speed) * forward.y * steered_right.y) +
          double(speed) * forward.x * steered_right.x);
    const float transfer_x = f32(transfer * steered_right.x), transfer_y = f32(transfer * steered_right.y);
    const double lateral =
        -((transfer * steered_right.z * right.z + double(transfer_y) * right.y) + double(transfer_x) * right.x);
    const float stored_lateral = f32(lateral);
    const VehicleVector correction{f32(-lateral * right.x), f32(-lateral * right.y), f32(-lateral * right.z), 0};
    const float original_side =
        f32((double(right.x) * s.velocity.x + double(right.y) * s.velocity.y) + double(right.z) * s.velocity.z);
    const float front_lateral = f32((1.0 - p.com_a) * stored_lateral + original_side),
                rear_lateral = f32((1.0 - p.com_b) * stored_lateral + original_side);
    s.wheels[0].lateral_speed = s.wheels[1].lateral_speed = rear_lateral;
    s.wheels[2].lateral_speed = s.wheels[3].lateral_speed = front_lateral;
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &w = s.wheels[i];
        w.grip_usage = 0;
        if (w.load <= 8.515625f)
            continue;
        auto &force = i < 2 ? rear_force : front_force;
        const float longitudinal = i < 2 ? speed : front_speed;
        float slip = f32(double(w.speed) - longitudinal);
        tyre_friction(p, s, i, w.lateral_speed, slip);
        slip = f32(double(slip) * 0.5);
        w.speed = f32(double(w.speed) - slip);
        force = f32(double(slip) + force);
        rolling_resistance(p, s, i, longitudinal, force);
        s.drive_force = f32(double(w.rolling_force) + s.drive_force);
        wheel_rotation(s, support, i, context.inverse_gravity);
    }
    front_force = f32(double(front_force) * 0.5);
    rear_force = f32(double(rear_force) * 0.5);
    const float rear_side = f32((double(s.wheels[1].lateral_speed) + s.wheels[0].lateral_speed) * 0.5);
    const float front_side = f32((double(s.wheels[3].lateral_speed) + s.wheels[2].lateral_speed) * 0.5);
    aerodynamic_drag_retained(p, s, front_force, rear_force, source_drag_adjustment);
    front_force = f32(double(speed) + front_force);
    rear_force = f32(double(speed) + rear_force);
    s.front_axle_velocity.x = f32(double(front_force) * forward.x + (double(front_side) * right.x + correction.x));
    s.front_axle_velocity.y = f32(double(f32(double(front_force) * forward.y)) +
                                  f32(double(f32(double(front_side) * right.y)) + correction.y));
    s.front_axle_velocity.z = f32(double(f32(double(front_force) * forward.z)) +
                                  f32(double(f32(double(front_side) * right.z)) + correction.z));
    s.rear_axle_velocity.x = f32(double(f32(double(rear_force) * forward.x)) + f32(double(rear_side) * right.x));
    s.rear_axle_velocity.y = f32(double(f32(double(rear_force) * forward.y)) + f32(double(rear_side) * right.y));
    s.rear_axle_velocity.z = f32(double(f32(double(rear_force) * forward.z)) + f32(double(rear_side) * right.z));
    bank_displacement(p, s, context.gravity);
    auto advance = [](VehicleVector &at, VehicleVector by) {
        at.x = f32(double(by.x) + at.x);
        at.y = f32(double(by.y) + at.y);
        at.z = f32(double(by.z) + at.z);
    };
    advance(s.front_axle_position, s.front_axle_velocity);
    advance(s.rear_axle_position, s.rear_axle_velocity);
    forward = vehicle_normalize({f32(double(s.front_axle_position.x) - s.rear_axle_position.x),
                                 f32(double(s.front_axle_position.y) - s.rear_axle_position.y),
                                 f32(double(s.front_axle_position.z) - s.rear_axle_position.z), forward.w});
    const float right_w = right.w;
    right = vehicle_normalize(vehicle_cross(forward, up));
    right.w = right_w;
    s.orientation = vehicle_basis(right, up, forward);
    s.basis_x = right;
    s.basis_z = forward;
    s.previous_velocity = s.velocity;
    s.previous_velocity_w = s.velocity_w;
    s.velocity.x = f32((double(s.rear_axle_velocity.x) + s.front_axle_velocity.x) * 0.5);
    s.velocity.y = f32(double(f32(double(s.rear_axle_velocity.y) + s.front_axle_velocity.y)) * 0.5);
    s.velocity.z = f32(double(f32(double(s.rear_axle_velocity.z) + s.front_axle_velocity.z)) * 0.5);
    const double vertical = (double(up.z) * s.previous_velocity.z + double(up.x) * s.previous_velocity.x) +
                            double(up.y) * s.previous_velocity.y;
    s.velocity.x = f32(double(f32(double(up.x) * vertical)) + s.velocity.x);
    s.velocity.y = f32(double(f32(double(up.y) * vertical)) + s.velocity.y);
    s.velocity.z = f32(vertical * up.z + s.velocity.z);
    s.acceleration = {f32(double(s.velocity.x) - s.previous_velocity.x),
                      f32(double(s.velocity.y) - s.previous_velocity.y),
                      f32(double(s.velocity.z) - s.previous_velocity.z)};
    s.speed = s.contact_class == 1
                  ? f32(std::sqrt((double(s.velocity.x) * s.velocity.x + double(s.velocity.y) * s.velocity.y) +
                                  double(s.velocity.z) * s.velocity.z))
                  : f32((double(forward.x) * s.velocity.x + double(forward.y) * s.velocity.y) +
                        double(forward.z) * s.velocity.z);
    for (auto &w : s.wheels)
        w.slip = std::abs(double(s.speed)) < double(1e-5f) ? 0 : f32(double(w.speed) / s.speed - 1);
    acceleration_tilt(p, s);
    aerodynamic_lift(p, s);
    suspension_tick(p, s, context.tick, random);
    attenuate_steering(p, s);
    airborne_wheel_decay(s);
    clear_wheel_acceleration(s);
}
} // namespace sgn::retail
