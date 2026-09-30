#include "sgn/vehicle_ai.hpp"
#include "sgn/vehicle_math.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
double d(float value)
{
    return static_cast<double>(value);
}
std::int32_t add_wrap(std::int32_t a, std::uint32_t b)
{
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) + b);
}
const AiRacer &opponent(const AiField &field, std::optional<std::size_t> index, std::size_t self)
{
    if (!index || *index >= field.size() || *index == self)
        throw std::domain_error("retail AI requires a valid opposing racer slot");
    return field[*index];
}
Vec3 stored_delta(const Vec3 &a, const Vec3 &b)
{
    return {f32(d(a.x) - b.x), f32(d(a.y) - b.y), f32(d(a.z) - b.z)};
}
// Preserve the original x87 accumulation order; no SIMD reassociation/FMA.
double dot_zyx(const Vec3 &a, const Vec3 &b)
{
    return (d(a.z) * b.z + d(a.y) * b.y) + d(a.x) * b.x;
}
double dot_xyz(const Vec3 &a, const Vec3 &b)
{
    return (d(a.x) * b.x + d(a.y) * b.y) + d(a.z) * b.z;
}
double progress_remainder(float progress)
{
    if (!std::isfinite(progress))
        throw std::domain_error("non-finite AI route progress");
    return std::fmod(d(progress), 100.0); // 0046291a: original CRT fmod dispatcher
}
} // namespace

std::optional<std::size_t> ai_select_ahead(const AiField &field, std::size_t self)
{
    const float own = f32(progress_remainder(field.at(self).route_progress));
    std::optional<std::size_t> ahead, wrapped;
    float positive = 1000, negative = 1000;
    for (std::size_t i = 0; i < field.size(); ++i)
    {
        if (i == self || field[i].active != 1)
            continue;
        const double delta = progress_remainder(field[i].route_progress) - own;
        if (delta >= 0)
        {
            if (delta < positive)
            {
                positive = f32(delta);
                ahead = i;
            }
        }
        else if (delta < negative)
        {
            negative = f32(delta);
            wrapped = i;
        }
    }
    return ahead ? ahead : wrapped;
}
std::optional<std::size_t> ai_select_behind(const AiField &field, std::size_t self)
{
    double remainder = progress_remainder(field.at(self).route_progress);
    float own = f32(remainder);
    if (remainder < 0)
        own = f32(d(own) + 100);
    std::optional<std::size_t> behind, wrapped;
    float positive = 1000, negative = 1000;
    for (std::size_t i = 0; i < field.size(); ++i)
    {
        if (i == self || field[i].active != 1)
            continue;
        // The EXE adds 100 BEFORE remainder for a negative candidate, while
        // adding it AFTER remainder for self. Preserve this asymmetry.
        const double p = field[i].route_progress;
        if (!std::isfinite(p))
            throw std::domain_error("non-finite AI route progress");
        const double delta = d(own) - std::fmod(p < 0 ? p + 100 : p, 100.0);
        if (delta >= 0)
        {
            if (delta < positive)
            {
                positive = f32(delta);
                behind = i;
            }
        }
        else if (delta < negative)
        {
            negative = f32(delta);
            wrapped = i;
        }
    }
    return behind ? behind : wrapped;
}
void ai_choose_passing_side(AiField &field, std::size_t self)
{
    auto &s = field.at(self);
    const auto &other = opponent(field, s.passing_target, self);
    const Vec3 delta = stored_delta(other.position, s.position);
    // Z remains in x87 after FST at 0040741c; its pre-store value is used here.
    const double along =
        ((d(other.position.z) - s.position.z) * s.forward.z + d(delta.y) * s.forward.y) + d(delta.x) * s.forward.x;
    if (along < 500)
        return;
    const double side = ((d(s.position.x) - s.route_position.x) * s.route_right.x +
                         (d(s.position.y) - s.route_position.y) * s.route_right.y) +
                        (d(s.position.z) - s.route_position.z) * s.route_right.z;
    if (std::abs(side) >= 1000)
        s.passing_offset = side > 0 ? 500.0f : -500.0f;
    else
        s.passing_offset = dot_xyz(delta, s.route_right) > -200 ? 500.0f : -500.0f;
}
void ai_follow_rival(AiField &field, std::size_t self, AiTactics &a)
{
    auto &s = field.at(self);
    if (s.following_ticks > 0)
        --s.following_ticks;
    if (s.contact_class != 0 || std::abs(d(s.bend)) > 100 || s.speed < 41.666668f)
        return;
    const auto &other = opponent(field, ai_select_ahead(field, self), self);
    const double speed_delta = d(other.speed) - s.speed;
    if (speed_delta > 16.666666f || speed_delta < -33.333332f)
        return;
    const double scale = d(a.spline_length) * d(0.01f);
    const double distance = scale * other.route_progress - scale * s.route_progress;
    if (distance > 1000 && distance < 10000 && s.following_ticks < 240)
        s.following_ticks = add_wrap(s.following_ticks, 2);
    const double x = d(other.position.x) - a.target.x;
    const double y = d(other.position.y) - a.target.y;
    const double z = d(other.position.z) - a.target.z;
    if (std::abs((z * s.route_right.z + y * s.route_right.y) + x * s.route_right.x) > 500)
        return;
    const float factor = f32(double(s.following_ticks) * d(0.004166667f));
    // Only X retains its unrounded subtraction/product through the final add.
    a.target.x = f32(d(a.target.x) + x * factor);
    a.target.y = f32(d(a.target.y) + f32(d(f32(y)) * factor));
    a.target.z = f32(d(a.target.z) + f32(d(f32(z)) * factor));
}
void ai_pass_rival(AiField &field, std::size_t self, AiTactics &a)
{
    auto &s = field.at(self);
    if (s.contact_class != 0)
        return;
    const auto chosen = ai_select_ahead(field, self);
    const auto &other = opponent(field, chosen, self);
    const Vec3 delta = stored_delta(other.position, s.position);
    const double speed_delta = d(s.speed) - other.speed;
    bool near = false;
    if (speed_delta < 5)
    {
        const double along = dot_zyx(delta, s.forward);
        if (s.passing_ticks > 30 && along >= -1000 && along < 500)
        {
            const double limit = d(other.speed) - d(8.333333f);
            if (limit < a.target_speed)
            {
                a.target_speed = f32(limit);
                s.throttle = 0;
            }
        }
    }
    else
    {
        double factor = speed_delta * d(0.023999998f);
        if (factor < 0.4f)
            factor = 0.4f;
        else if (factor > 1)
            factor = 1;
        const double along = dot_zyx(delta, s.forward);
        near = along >= -500 && along < factor * 3500 && std::abs(dot_zyx(delta, s.right)) < 300;
    }
    if (near)
    {
        s.passing_target = chosen;
        if (s.passing_ticks < 120)
            s.passing_ticks = add_wrap(s.passing_ticks, 6);
    }
    else
        s.passing_ticks = add_wrap(s.passing_ticks, 0xffffffffu);
    if (s.passing_ticks <= 0)
    {
        s.passing_ticks = 0;
        return;
    }
    --s.passing_ticks;
    ai_choose_passing_side(field, self);
    const auto &target = opponent(field, s.passing_target, self);
    const double x = d(s.passing_offset) * target.right.x + target.position.x - a.target.x;
    const float y = f32(d(f32(d(f32(d(s.passing_offset) * target.right.y)) + target.position.y)) - a.target.y);
    const float z = f32(d(f32(d(f32(d(s.passing_offset) * target.right.z)) + target.position.z)) - a.target.z);
    const double factor = double(s.passing_ticks) * d(0.008333334f);
    const float stored_factor = f32(factor);
    a.target.x = f32(d(a.target.x) + x * factor);
    a.target.y = f32(d(a.target.y) + f32(d(stored_factor) * y));
    a.target.z = f32(d(a.target.z) + f32(d(stored_factor) * z));
}
void ai_yield_to_rival(AiField &field, std::size_t self, AiTactics &a)
{
    auto &s = field.at(self);
    if (s.contact_class != 0)
        return;
    const auto chosen = ai_select_behind(field, self);
    if (std::abs(d(s.bend)) > 25)
        return;
    const auto &other = opponent(field, chosen, self);
    if (d(other.speed) - s.speed < 1.6666666f)
        return;
    const std::uint32_t threshold = (std::bit_cast<std::uint32_t>(s.race_index) + 1u) * 3u;
    if (s.rival_counters.at(*chosen) < threshold)
        return;
    const Vec3 delta = stored_delta(other.position, s.position);
    const double distance = (d(other.route_progress) - s.route_progress) * a.course_length * d(0.01f);
    if (distance < -1500 || distance > 0)
        return;
    const double offset =
        ((d(delta.y) * s.route_right.y + d(delta.z) * s.route_right.z) + d(delta.x) * s.route_right.x) * 2;
    const float x = f32(offset * s.route_right.x), y = f32(offset * s.route_right.y);
    const double z = offset * s.route_right.z;
    a.response_scale = f32(d(a.response_scale) * 5);
    a.target.x = f32(d(a.target.x) + x);
    a.target.y = f32(d(a.target.y) + y);
    a.target.z = f32(d(a.target.z) + z);
}
void ai_steer_to_target(AiField &field, std::size_t self, AiTactics &a)
{
    auto &s = field.at(self);
    if (s.gear == 0)
        return;
    const double x = d(s.position.x) - a.target.x;
    const float y = f32(d(s.position.y) - a.target.y), z = f32(d(s.position.z) - a.target.z);
    const double height = (d(a.world_up.z) * z + d(a.world_up.y) * y) + d(a.world_up.x) * x;
    const float px = f32(d(a.world_up.x) * height), py = f32(d(a.world_up.y) * height),
                pz = f32(d(a.world_up.z) * height);
    const double side = ((d(pz) + z) * s.right.z + (d(py) + y) * s.right.y) + (x + px) * s.right.x;
    double denominator = d(s.steering_divisor_signal) * s.driving_factor * d(0.35f);
    if (denominator < 1)
        denominator = 1;
    double change = side * d(0.0005f) * d(2.25f) / denominator;
    if (s.up_y < 0)
        change = -change;
    if (s.steering_ramp_ticks != 0)
    {
        // 00406814 SUB + FILD qword with zero upper DWORD: unsigned 32-bit
        // wraparound, despite signed arithmetic in the Ghidra pseudocode.
        const auto remaining = 300u - std::bit_cast<std::uint32_t>(s.steering_ramp_ticks);
        const double factor = double(remaining) * d(0.0033333334f);
        a.response_scale = f32(factor * factor * a.response_scale);
    }
    const float step = f32(d(a.response_scale) * change);
    // Shared, independently qualified 004143e0/00414420 preserve the live
    // binary53 limit comparison before the stored binary32 result.
    if (step >= 0)
        vehicle_steer_increase(s.steering, s.max_steering, step);
    else
        vehicle_steer_decrease(s.steering, s.max_steering, -step);
}
void ai_recover_stalled(AiField &field, std::size_t self)
{
    auto &s = field.at(self);
    const auto next = std::bit_cast<std::uint32_t>(s.reverse_ticks) + 1u;
    s.reverse_ticks = std::bit_cast<std::int32_t>(next);
    if (std::abs(d(s.speed)) <= 8.333333f)
    {
        if (s.throttle <= 0.1f || next < 301u)
            return;
        s.gear = s.gear != 0 ? 0 : 2;
    }
    else
    {
        if (next < 120u)
            return;
        if (s.gear == 0)
            s.gear = 2;
    }
    s.reverse_ticks = 0;
}
} // namespace sgn::retail
