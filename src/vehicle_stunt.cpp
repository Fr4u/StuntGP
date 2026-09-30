#include "sgn/vehicle.hpp"
#include "sgn/vehicle_stunt_pattern.hpp"

namespace sgn::retail
{
namespace
{
double dot_zyx(Vec3 a, Vec3 b)
{
    return (double(a.z) * b.z + double(a.y) * b.y) + double(a.x) * b.x;
}
Vec3 xyz(VehicleVector v)
{
    return {v.x, v.y, v.z};
}
void negative_rotation(float &acceleration, float &speed, float configured_max, float mass)
{
    const double next_acceleration = double(acceleration) - 0.8f;
    acceleration = vehicle_f32(next_acceleration);
    if (next_acceleration < -double(0.8f))
        acceleration = -0.8f;
    const double next_speed = double(speed) + acceleration;
    speed = vehicle_f32(next_speed);
    double limit = double(configured_max) / mass;
    if (limit > 12)
        limit = 12;
    const float negative_limit = vehicle_f32(-limit);
    if (next_speed < negative_limit)
        speed = negative_limit;
}
void positive_rotation(float &acceleration, float &speed, float configured_max, float mass)
{
    const double next_acceleration = double(acceleration) + 0.8f;
    acceleration = vehicle_f32(next_acceleration);
    if (next_acceleration > 0.8f)
        acceleration = 0.8f;
    speed = vehicle_f32(double(speed) + acceleration);
    double limit = double(configured_max) / mass;
    if (limit > 12)
        limit = 12;
    // The positive entry reloads the rounded speed; its negative counterpart
    // compares the retained x87 value against a rounded negative limit.
    if (speed > limit)
        speed = vehicle_f32(limit);
}
void stunt_input(const VehicleParameters &p, VehicleState &s, std::uint32_t tick, Vec3 gravity, std::uint8_t threshold)
{
    if (s.input.stunt != 0 && s.previous_stunt == 0)
        s.stunt.launch_tick = tick;
    if (s.contact_counter != 0 || s.stunt.active == 0)
        return;
    bool pitch_processed = false;
    if (std::abs(s.stunt.angular_velocity.y) < 0.2f)
    {
        if (s.input.brake > threshold)
        {
            positive_pitch_input(p, s);
            pitch_processed = true;
        }
        else if (s.input.throttle > threshold)
        {
            negative_pitch_input(p, s);
            pitch_processed = true;
        }
    }
    if (std::abs(s.stunt.angular_velocity.x) < 0.2f)
    {
        if (s.input.left > threshold)
        {
            positive_yaw_input(p, s);
            return;
        }
        if (s.input.right > threshold)
        {
            negative_yaw_input(p, s);
            return;
        }
    }
    if (pitch_processed)
        return;
    clear_angular_acceleration(s);
    assist_air_pitch(s, gravity);
    assist_air_yaw(s);
}
} // namespace
void reset_balance_counters(VehicleState &s) noexcept
{
    for (unsigned i = 256; i < 260; ++i)
        s.stunt.history_and_balance[i] = 0;
}
void reset_flip_state(VehicleState &target, VehicleState &current) noexcept
{
    // ECX owns the phase/timer; the EXE's global current owns only XYZ.
    target.stunt.flip_phase = 0;
    target.stunt.flip_timer = 0;
    current.stunt.flip_up.x = 0;
    current.stunt.flip_up.y = 0;
    current.stunt.flip_up.z = 0;
}
void reset_drift_counter(VehicleState &s) noexcept
{
    s.stunt.drift_ticks = 0;
}
void reset_slipstream_counter(VehicleState &s) noexcept
{
    s.stunt.slipstream_ticks = 0;
}
void reset_roof_counter(VehicleState &s) noexcept
{
    s.stunt.roof_ticks = 0;
}
void reset_inversion_state(VehicleState &s) noexcept
{
    s.stunt.inversion_latch = 0;
    s.stunt.inversion_timer = 0;
}
void reset_phase_history(VehicleState &s) noexcept
{
    s.stunt.history_and_balance[0] = 0;
    s.stunt.phase_indices[0] = 0;
    s.stunt.history_and_balance[64] = 0;
    s.stunt.phase_indices[1] = 0;
}
void reset_air_rotation(VehicleState &s) noexcept
{
    s.stunt.active = 0;
    s.airborne_ticks = 0;
    s.stunt.angular_acceleration.x = 0;
    s.stunt.angular_acceleration.y = 0;
    s.stunt.angular_acceleration.z = 0;
    s.stunt.angular_velocity.x = 0;
    s.stunt.angular_velocity.y = 0;
    s.stunt.angular_velocity.z = 0;
}
void reset_stunt_state(VehicleState &target, VehicleState &current) noexcept
{
    reset_balance_counters(target);
    reset_flip_state(target, current);
    reset_drift_counter(target);
    reset_slipstream_counter(target);
    reset_roof_counter(target);
    reset_inversion_state(target);
    reset_phase_history(current);
    reset_air_rotation(target);
    target.stunt.previous_active = 0;
}
void reset_stunt_score_hold(VehicleState &s) noexcept
{
    s.stunt.score_hold = 0;
}
void begin_stunt_landing(VehicleState &s) noexcept
{
    s.stunt.landing_pending = 1;
    s.stunt.landing_counter = 0;
}
void reset_launch_state(VehicleState &s) noexcept
{
    s.stunt.launch_state = 0;
}
void begin_stunt_height(VehicleState &s) noexcept
{
    s.stunt.launch_height = s.position.y;
    s.stunt.maximum_height = 0;
    s.stunt.height_level = 0;
}
void negative_pitch_input(const VehicleParameters &p, VehicleState &s)
{
    negative_rotation(s.stunt.angular_acceleration.x, s.stunt.angular_velocity.x, p.max_pitch_speed, p.mass);
}
void positive_pitch_input(const VehicleParameters &p, VehicleState &s)
{
    positive_rotation(s.stunt.angular_acceleration.x, s.stunt.angular_velocity.x, p.max_pitch_speed, p.mass);
}
void positive_yaw_input(const VehicleParameters &p, VehicleState &s)
{
    positive_rotation(s.stunt.angular_acceleration.y, s.stunt.angular_velocity.y, p.max_yaw_speed, p.mass);
}
void negative_yaw_input(const VehicleParameters &p, VehicleState &s)
{
    negative_rotation(s.stunt.angular_acceleration.y, s.stunt.angular_velocity.y, p.max_yaw_speed, p.mass);
}
void clear_angular_acceleration(VehicleState &s) noexcept
{
    s.stunt.angular_acceleration.x = 0;
    s.stunt.angular_acceleration.y = 0;
    s.stunt.angular_acceleration.z = 0;
}
void assist_air_pitch(VehicleState &s, Vec3 gravity)
{
    if (dot_zyx(gravity, s.up) >= 0 || std::abs(s.stunt.angular_velocity.x) < 0.05f)
        return;
    double value = std::abs(dot_zyx(gravity, xyz(s.basis_z)));
    if (s.stunt.angular_velocity.x < 0)
        value = -value;
    s.stunt.angular_velocity.x = vehicle_f32(value * 6);
}
void assist_air_yaw(VehicleState &s)
{
    if (dot_zyx(xyz(s.basis_z), s.velocity) <= 0 || std::abs(s.stunt.angular_velocity.y) < 0.05f)
        return;
    const auto direction = vehicle_normalize({s.velocity.x, s.velocity.y, s.velocity.z, s.velocity_w});
    double value = std::abs(dot_zyx(xyz(direction), xyz(s.basis_x)));
    if (s.stunt.angular_velocity.y < 0)
        value = -value;
    s.stunt.angular_velocity.y = vehicle_f32(value * 6);
}
void digital_stunt_input(const VehicleParameters &p, VehicleState &s, std::uint32_t tick, Vec3 gravity)
{
    stunt_input(p, s, tick, gravity, 0);
}
void analog_stunt_input(const VehicleParameters &p, VehicleState &s, std::uint32_t tick, Vec3 gravity)
{
    stunt_input(p, s, tick, gravity, 127);
}
void save_phase_history(VehicleState &s)
{
    auto &t = s.stunt;
    if (t.phase_indices[0] > 64 || t.phase_indices[1] > 64)
        throw std::domain_error("stunt history exceeds recovered capacity");
    for (std::int32_t i = 0; i <= t.phase_indices[0]; ++i)
        t.history_and_balance[128 + i] = t.history_and_balance[i];
    for (std::int32_t i = 0; i <= t.phase_indices[1]; ++i)
        t.history_and_balance[192 + i] = t.history_and_balance[64 + i];
    t.phase_indices[2] = t.phase_indices[0];
    t.phase_indices[3] = t.phase_indices[1];
    reset_phase_history(s);
}
void update_phase_history(VehicleState &s, Vec3 gravity)
{
    auto &t = s.stunt;
    if (t.phase_indices[0] < 0 || t.phase_indices[0] > 64 || t.phase_indices[1] < 0 || t.phase_indices[1] > 64)
        throw std::domain_error("stunt phase index outside recovered range");
    auto sector = [](double transverse, double facing) -> std::uint32_t {
        constexpr float boundary = std::bit_cast<float>(std::uint32_t{0x3f3504f4});
        if (facing < 0)
        {
            if (transverse < 0)
                return transverse <= -double(boundary) ? 6u : 5u;
            return transverse < boundary ? 4u : 3u;
        }
        if (transverse < 0)
            return transverse <= -double(boundary) ? 7u : 8u;
        return transverse < boundary ? 1u : 2u;
    };
    auto append = [&](unsigned axis, std::uint32_t phase) {
        auto &index = t.phase_indices[axis];
        if (t.history_and_balance[axis * 64 + index] != phase && index < 64)
        {
            ++index;
            t.history_and_balance[axis * 64 + index] = phase;
        }
    };
    append(0, sector(-dot_zyx(gravity, xyz(s.basis_z)), -dot_zyx(gravity, s.up)));
    const double projection =
        (double(gravity.y) * s.velocity.y + double(gravity.z) * s.velocity.z) + double(gravity.x) * s.velocity.x;
    // X/Y projected components are stored before subtraction; Z stays in x87.
    const auto horizontal =
        vehicle_normalize({vehicle_f32(double(s.velocity.x) - vehicle_f32(double(gravity.x) * projection)),
                           vehicle_f32(double(s.velocity.y) - vehicle_f32(double(gravity.y) * projection)),
                           vehicle_f32(double(s.velocity.z) - projection * gravity.z), 0});
    const auto side = vehicle_normalize(vehicle_cross(horizontal, {gravity.x, gravity.y, gravity.z, 0}));
    auto forward_dot = [&](VehicleVector v) {
        return (double(v.x) * s.basis_z.x + double(v.y) * s.basis_z.y) + double(v.z) * s.basis_z.z;
    };
    append(1, sector(forward_dot(side), forward_dot(horizontal)));
}
void stop_all_wheels(VehicleState &s) noexcept
{
    for (auto &wheel : s.wheels)
    {
        wheel.speed = 0;
        wheel.acceleration = 0;
    }
}
void reset_vehicle_motion(VehicleState &s) noexcept
{
    stop_all_wheels(s);
    s.total_speed = 0;
    s.speed = 0;
    s.rpm = 0;
    s.previous_velocity = {};
    s.velocity = {};
    s.acceleration = {};
    s.incremental = vehicle_identity();
    s.secondary_rotation = vehicle_identity();
}
void accumulate_air_score(VehicleState &s, std::int32_t game_mode)
{
    if (game_mode != 2)
    {
        s.air_score_rate = 0;
        return;
    }
    if (s.contact_counter == 0)
    {
        if (s.air_score_rate < 6.4f)
            s.air_score_rate = vehicle_f32(double(s.air_score_rate) + 0.025f);
    }
    else
    {
        const double fraction = double(s.air_score_rate) * 0.25f;
        s.air_score_rate =
            vehicle_f32(fraction > 0.08f ? double(s.air_score_rate) * 0.9f : (1 - fraction) * s.air_score_rate);
        if (s.air_score_rate < 0.01f)
            s.air_score_rate = 0;
    }
    const double score = double(s.score) + s.air_score_rate;
    s.score = vehicle_f32(score);
    if (score > 9999999.0f)
        s.score = 9999999.0f;
}
void update_stunt_chain_decay(VehicleState &s) noexcept
{
    auto &t = s.stunt;
    auto negative = [](std::uint32_t value) { return std::bit_cast<std::int32_t>(value) < 0; };
    if (s.contact_counter != 0 && !negative(t.score_hold))
        --t.score_hold;
    if (t.chain_count != 0 && negative(t.score_hold))
    {
        t.chain_decay_active = 1;
        t.chain_decay_timer = 0;
        t.score_hold = 0;
        t.chain_count = 0;
    }
    if (t.chain_decay_active == 1 && negative(--t.chain_decay_timer))
    {
        if (t.displayed_chain_count == 0)
        {
            t.displayed_chain_count = 0;
            t.chain_decay_active = 0;
            return;
        }
        --t.displayed_chain_count;
        t.chain_decay_timer = 30;
    }
}
void steer_air_velocity(VehicleState &s, Vec3 gravity)
{
    if (s.contact_counter != 0 || s.stunt.active == 1)
        return;
    double steering = s.steering;
    if (steering > 5)
        steering = 5;
    else if (steering < -5)
        steering = -5;
    const float steering_scale = vehicle_f32(steering * 0.2f);
    float speed{};
    const auto direction = vehicle_normalize({s.velocity.x, s.velocity.y, s.velocity.z, s.velocity_w}, &speed);
    if (speed < 1)
        return;
    const double side_x = double(gravity.z) * direction.y - double(gravity.y) * direction.z;
    const float side_y = vehicle_f32(double(gravity.x) * direction.z - double(gravity.z) * direction.x);
    const double side_z = double(gravity.y) * direction.x - double(gravity.x) * direction.y;
    const double scaled_speed = double(speed) * std::bit_cast<float>(std::uint32_t{0x3c449ba5});
    speed = vehicle_f32(scaled_speed);
    if (scaled_speed > 1)
        speed = 1;
    const double amount = double(speed) * steering_scale;
    const float x = vehicle_f32(amount * side_x), y = vehicle_f32(amount * side_y);
    s.velocity.x = vehicle_f32(double(x) + s.velocity.x);
    s.velocity.y = vehicle_f32(double(y) + s.velocity.y);
    s.velocity.z = vehicle_f32(amount * side_z + s.velocity.z);
}
void record_stunt_statistic(VehicleStatistic &statistic, float value)
{
    const double total = double(value) + statistic.total;
    ++statistic.count;
    statistic.last = value;
    statistic.total = vehicle_f32(total);
    statistic.average = vehicle_f32(total / double(statistic.count));
}
void record_stunt_height(VehicleState &s)
{
    const double height = double(s.stunt.maximum_height) - s.stunt.launch_height;
    if (height >= 0)
        record_stunt_statistic(s.stunt_statistics[3], vehicle_f32(height));
}
void set_launch_effect(VehicleState &s, float strength)
{
    const double scaled = double(strength) * 255.0f;
    if (!std::isfinite(scaled) || scaled < -9223372036854775808.0 || scaled >= 9223372036854775808.0)
        throw std::domain_error("launch effect outside recovered conversion range");
    const auto level = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled)));
    if (level >= s.launch_effect_level)
        s.launch_effect_level = level;
    s.launch_effect_timer = 30;
    s.launch_effect_strength = vehicle_f32(double(strength) + strength);
}
void launch_stunt(const VehicleParameters &p, VehicleState &s, std::uint32_t tick)
{
    auto &t = s.stunt;
    if (t.previous_contact_counter == 0 || t.landing_hold != 0)
        return;
    t.accumulated_angles.x = 0;
    t.accumulated_angles.y = 0;
    t.accumulated_angles.z = 0;
    s.launch_score = s.score;
    if (t.launch_tick == 0)
        return;
    const double distance = std::abs(double(std::bit_cast<std::int32_t>(t.launch_tick - tick)));
    const float window = vehicle_f32(double(p.launch_time) * 60.0f);
    if (distance >= window)
        return;
    double adjusted = distance - double(p.launch_tolerance) * 60.0f;
    if (adjusted < 0)
        adjusted = 0;
    float strength = vehicle_f32((double(window) - adjusted) / window);
    record_stunt_statistic(s.stunt_statistics[0], vehicle_f32(double(strength) * 100.0f));
    strength = vehicle_f32(double(strength) * strength);
    set_launch_effect(s, strength);
    double lift = double(strength) * p.launch_speed / s.effective_mass;
    if (lift > 25)
        lift = 25;
    s.velocity.y = vehicle_f32(lift + s.velocity.y);
    s.launch_event = 1;
    s.launch_event_strength = strength;
    t.active = 1;
    if (t.chain_count < 10)
        ++t.chain_count;
    if (std::bit_cast<std::int32_t>(t.score_hold) < 0)
        t.score_hold = 270;
    else
    {
        t.score_hold += 270;
        if (std::bit_cast<std::int32_t>(t.score_hold) > 600)
            t.score_hold = 600;
    }
}
void reset_stunt_meter(VehicleState &s) noexcept
{
    s.stunt_meter = 0;
}
void fail_stunt_landing(VehicleState &s)
{
    auto &t = s.stunt;
    t.landing_counter = 0;
    t.landing_hold = 0;
    t.landing_pending = 0;
    record_stunt_height(s);
    record_stunt_statistic(s.stunt_statistics[1], 0);
    record_stunt_statistic(s.stunt_statistics[2], vehicle_f32(double(s.score) - s.launch_score));
    s.stunt_statistics[2].average = vehicle_f32(double(s.score) / s.stunt_statistics[2].count);
    t.score_hold = 0;
    if (t.chain_count != 0)
        --t.chain_count;
    reset_stunt_meter(s);
}
unsigned match_stunt_pattern(const VehicleState &s, const VehicleStuntPattern &pattern)
{
    const auto &t = s.stunt;
    const unsigned axis = pattern.pitch[0] == -1 ? 1u : 0u;
    const auto &sequence = axis == 0 ? pattern.pitch : pattern.yaw;
    if (sequence[0] == -1 || t.phase_indices[2 + axis] < 1)
        return 0;
    if (t.phase_indices[2 + axis] > 64)
        throw std::domain_error("saved stunt history exceeds recovered capacity");
    unsigned position = 0, matches = 0, repetitions = 0;
    for (std::int32_t i = 1; i <= t.phase_indices[2 + axis]; ++i)
    {
        const auto phase = std::bit_cast<std::int32_t>(t.history_and_balance[128 + axis * 64 + i]);
        if (phase == sequence.at(position))
        {
            ++position;
            ++matches;
            if (sequence.at(position) == -1)
            {
                position = 1;
                ++repetitions;
            }
        }
        else
        {
            if (position >= 4)
                return 0;
            position = 0;
            if (phase != sequence[0])
            {
                do
                {
                    if (position >= 4)
                        return 0;
                    ++position;
                } while (phase != sequence.at(position));
                if (position >= 4)
                    return 0;
            }
            ++position;
        }
    }
    if (matches != 0 && static_cast<std::int32_t>(matches) <= t.phase_indices[2 + (1 - axis)])
        return 0;
    return repetitions > 3 ? 3 : repetitions;
}
} // namespace sgn::retail
