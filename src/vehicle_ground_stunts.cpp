#include "sgn/vehicle_ground_stunts.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
void reward(VehicleState &state, const VehicleGroundStuntContext &c, unsigned kind, unsigned category, unsigned variant,
            int priority)
{
    if (c.current >= 6 || kind >= c.rewards.values.size())
        throw FormatError("ground stunt identity");
    const auto &award = c.rewards.values[kind];
    award_stunt_score(state, c.controls, c.current, c.score, c.stunt, category, variant, award.points);
    contact::player::post_notice(c.controls, c.stunt.game_mode, c.stunt.tick, 180, award.text, c.current, 2, priority);
}
using sgn::bit_reinterpret::compile_time::as_float;
double dot(VehicleVector a, Vec3 b)
{
    return (double(a.z) * b.z + double(a.y) * b.y) + double(a.x) * b.x;
}
} // namespace
void award_vehicle_balance(VehicleState &state, const VehicleGroundStuntContext &c, unsigned kind)
{
    if (kind >= 4 || c.current >= 6)
        throw FormatError("balance award identity");
    if (contact::player::find_controller(c.controls, c.current))
        reward(state, c, kind, 5, 0, 0);
}
void update_vehicle_balance(VehicleState &s, const VehicleGroundStuntContext &c)
{
    auto &counter = s.stunt.history_and_balance;
    if (s.contact_class == 0)
    {
        if (counter[259] > 30)
            award_vehicle_balance(s, c, 0);
        else if (counter[258] > 30)
            award_vehicle_balance(s, c, 1);
        else if (counter[257] > 60)
            award_vehicle_balance(s, c, 2);
        else if (counter[256] > 60)
            award_vehicle_balance(s, c, 3);
        reset_balance_counters(s);
        return;
    }
    if (!(s.speed >= as_float(0x40555555u)))
        return;
    const auto a = s.wheels[0].contact_countdown, b = s.wheels[1].contact_countdown;
    const auto d = s.wheels[2].contact_countdown, e = s.wheels[3].contact_countdown;
    if (d <= 0 && e <= 0 && a > 2 && b > 2)
        ++counter[259];
    if (a <= 0)
    {
        if (b <= 0 && d > 2 && e > 2)
            ++counter[258];
        if (d <= 0 && b > 2 && e > 2)
            ++counter[257];
    }
    if (b <= 0 && e <= 0 && a > 2 && d > 2)
        ++counter[256];
}
void update_vehicle_flip(VehicleState &s, const VehicleGroundStuntContext &c)
{
    auto &t = s.stunt;
    if (t.flip_timer != 0 && --t.flip_timer == 0)
        reset_flip_state(s, s);
    if (s.contact_class == 0 && t.flip_phase == 3 && t.flip_timer != 0)
    {
        reward(s, c, 4, 5, 0, 100);
        reset_flip_state(s, s);
    }
    if (s.previous_contact_class == 0)
    {
        if (s.contact_class == 0)
        {
            if (t.flip_phase != 3)
                reset_flip_state(s, s);
        }
        else
        {
            t.flip_up = {s.up.x, s.up.y, s.up.z, s.basis_y_w};
            t.flip_phase = 1;
        }
    }
    else if (s.previous_contact_class == 1)
    {
        if (s.contact_class == 1)
        {
            if (t.flip_phase == 1 && !(dot(t.flip_up, s.up) >= as_float(0xbf34fdf4u)))
                t.flip_phase = 2;
        }
        else if (t.flip_phase == 2)
        {
            t.flip_phase = 3;
            t.flip_timer = 300;
        }
    }
    if (t.flip_phase == 2 && !(dot(s.basis_z, s.velocity) >= as_float(0x3f34fdf4u)))
        reset_flip_state(s, s);
}
void update_vehicle_drift(VehicleState &s, const VehicleGroundStuntContext &c)
{
    if (s.contact_class != 1)
    {
        float length{};
        const auto direction = vehicle_normalize({s.velocity.x, s.velocity.y, s.velocity.z, s.velocity_w}, &length);
        if (length >= as_float(0x41855555u))
        {
            const double alignment = dot(direction, {s.basis_z.x, s.basis_z.y, s.basis_z.z});
            if (!(alignment >= std::bit_cast<double>(std::uint64_t{0x3fed007901739d87})))
            {
                ++s.stunt.drift_ticks;
                return;
            }
            if (s.stunt.drift_ticks > 150)
                reward(s, c, 6, 5, 1, 0);
            else if (s.stunt.drift_ticks > 90)
                reward(s, c, 5, 4, 0, 0);
        }
    }
    reset_drift_counter(s);
}
void update_vehicle_slipstream(VehicleState &s, const VehicleGroundStuntContext &c)
{
    if (!(s.air_factor >= as_float(0x3f7d70a4u)))
    {
        ++s.stunt.slipstream_ticks;
        return;
    }
    if (s.stunt.slipstream_ticks > 540)
        reward(s, c, 8, 5, 1, 0);
    else if (s.stunt.slipstream_ticks > 300)
        reward(s, c, 7, 5, 1, 0);
    reset_slipstream_counter(s);
}
void update_vehicle_roof_slide(VehicleState &s, std::int32_t shock_hold, const VehicleGroundStuntContext &c)
{
    if (shock_hold != 0 && s.speed > as_float(0x41055555u))
    {
        ++s.stunt.roof_ticks;
        return;
    }
    if (s.stunt.roof_ticks > 120)
        reward(s, c, 9, 5, 1, 0);
    reset_roof_counter(s);
}
} // namespace sgn::retail
