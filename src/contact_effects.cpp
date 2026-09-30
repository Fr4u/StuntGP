#include "sgn/contact_effects.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace sgn::retail::contact::effects
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
} // namespace
bool find_contact_partner(std::span<PairVehicle, 6> vehicles, unsigned current, std::int32_t mode,
                          std::optional<unsigned> &selected)
{
    if (current >= 6)
        throw std::out_of_range("contact vehicle index");
    if (mode == 4)
        return false;
    selected.reset();
    const auto &a = vehicles[current];
    for (unsigned i = 0; i < 6; ++i)
    {
        if (i == current)
            continue;
        auto &b = vehicles[i];
        if (b.active != 1)
            continue;
        const double x = double(a.previous_position.x) - b.previous_position.x;
        const double y = double(a.previous_position.y) - b.previous_position.y;
        const double z = double(a.previous_position.z) - b.previous_position.z;
        if (!((z * z + y * y) + x * x < 90000.0))
            continue;
        if (b.cooldown == 0)
        {
            selected = i;
            return true;
        }
        if (b.cooldown < 10)
            b.cooldown += 60;
    }
    return false;
}
void mark_pair_contact(std::span<PairVehicle, 6> vehicles, unsigned first, unsigned second)
{
    if (first >= 6 || second >= 6)
        throw std::out_of_range("contact pair index");
    vehicles[first].counters.current[second] = 8;
    vehicles[second].counters.current[first] = 8;
}
void update_pair_counters(PairCounters &counters)
{
    for (unsigned i = 0; i < 6; ++i)
    {
        if (counters.previous[i] == 0 && counters.current[i] != 0)
            ++counters.count[i];
        counters.previous[i] = counters.current[i];
        if (counters.current[i] != 0)
            --counters.current[i];
    }
}
PairImpulse apply_pair_impulse(ContactBody &a, ContactBody &b, Vector4 &pose_a, Vector4 &pose_b)
{
    float distance{};
    const auto n =
        vehicle_normalize({f32(double(a.position.x) - b.position.x), f32(double(a.position.y) - b.position.y),
                           f32(double(a.position.z) - b.position.z), 0},
                          &distance);
    const float speed =
        f32(((double(b.velocity.z) - a.velocity.z) * n.z + (double(b.velocity.y) - a.velocity.y) * n.y) +
            (double(b.velocity.x) - a.velocity.x) * n.x);
    const double inverse_mass = 1.0 / (double(b.mass) + a.mass);
    const double overlap = (300.0 - distance) * double(0.0033333334140479565f);
    const auto apply = [&](Vector4 &velocity, double impulse) {
        const float x = f32(impulse * n.x), y = f32(impulse * n.y);
        const double z = impulse * n.z;
        velocity.x = f32(double(x) + velocity.x);
        velocity.y = f32(double(y) * 0.5 + velocity.y);
        velocity.z = f32(z + velocity.z);
    };
    apply(a.velocity, ((overlap * b.mass) * inverse_mass) * 25.0);
    apply(b.velocity, ((overlap * inverse_mass) * a.mass) * -25.0);
    pose_a = a.velocity;
    pose_b = b.velocity;
    return {{n.x, n.y, n.z}, distance, speed};
}
void emit_impact(ImpactParticles &state, VehicleRandom &random, const Vector4 &position, const Vector4 &velocity,
                 float strength)
{
    if (state.throttle > 0)
        return;
    state.throttle = state.rate_selector < 80 ? 2 : 1;
    if (state.cursor >= 32)
        state.cursor = 0;
    auto &particle = state.particles[state.cursor];
    particle.position = position;
    particle.type = 4;
    random.state = random.state * 0x343fdu + 0x269ec3u;
    const auto sample = static_cast<std::int32_t>((random.state >> 16) & 0x7fff) - 0x3fff;
    // 0042F1F0: signed division by 32767, truncating towards zero.
    particle.lifetime = (sample * 30) / 32767 + 60;
    particle.decay = std::bit_cast<float>(std::uint32_t{0x3f666666});
    particle.alpha = 1;
    particle.initial_lifetime = particle.lifetime;
    particle.field_430 = 0;
    particle.field_434 = 0;
    particle.scale = f32(std::abs(double(strength)) * 0.25 + 0.5);
    particle.velocity = velocity;
    const float x = f32(random.signed_value(std::bit_cast<float>(std::uint32_t{0x41055555})));
    const float y =
        f32(random.signed_value(std::bit_cast<float>(std::uint32_t{0x41855555})) + double(8.333333015441895f));
    const double z = random.signed_value(std::bit_cast<float>(std::uint32_t{0x41055555}));
    particle.velocity.x = f32(double(x) + particle.velocity.x);
    particle.velocity.y = f32(double(y) + particle.velocity.y);
    particle.velocity.z = f32(z + particle.velocity.z);
    ++state.cursor;
    ++state.emitted;
}
} // namespace sgn::retail::contact::effects
