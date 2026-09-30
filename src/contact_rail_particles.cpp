#include "sgn/contact_rail_particles.hpp"
#include <bit>

namespace sgn::retail::contact::effects
{
namespace
{
void initialize_refuel(ImpactParticle &p, const Vector4 &position)
{
    p.position = position;
    p.lifetime = 60;
    p.decay = std::bit_cast<float>(std::uint32_t{0x3f666666});
    p.alpha = 0;
    p.initial_lifetime = 0;
    p.field_430 = 0;
    p.field_434 = 0;
    p.scale = 1;
}
unsigned available_slot(ImpactParticles &state)
{
    state.throttle = state.rate_selector < 80 ? 4 : 2;
    if (state.cursor >= 32)
        state.cursor = 0;
    return state.cursor;
}
void finish(ImpactParticles &state)
{
    ++state.cursor;
    ++state.emitted;
}
} // namespace
std::optional<unsigned> emit_refuel_particle(ImpactParticles &state, VehicleRandom &random, const Vector4 &position,
                                             const Vector4 &velocity, Vec3 right)
{
    if (state.throttle > 0)
        return std::nullopt;
    const auto slot = available_slot(state);
    auto &p = state.particles[slot];
    const double noise = random.signed_value(std::bit_cast<float>(std::uint32_t{0x42855555}));
    const float x = vehicle_f32(noise * right.x), y = vehicle_f32(noise * right.y);
    p.velocity.x = vehicle_f32(double(x) + velocity.x);
    p.velocity.y = vehicle_f32(double(y) + velocity.y);
    p.velocity.z = vehicle_f32(noise * right.z + velocity.z); // fourth lane deliberately untouched
    initialize_refuel(p, position);
    random.state = random.state * 0x343fdu + 0x269ec3u;
    const auto sample = static_cast<std::int32_t>((random.state >> 16) & 0x7fff) - 0x3fff;
    const auto choice = (sample * 1024) / 32767;
    p.type = (choice & 3) == 0 ? 7 : (choice & 1) + 5;
    finish(state);
    return slot;
}
std::optional<unsigned> emit_refuel_anchor_particle(ImpactParticles &state, const Vector4 &position,
                                                    const Vector4 &velocity)
{
    if (state.throttle != 2)
        return std::nullopt;
    const auto slot = available_slot(state);
    auto &p = state.particles[slot];
    p.velocity = velocity;
    initialize_refuel(p, position);
    p.type = 5;
    finish(state);
    return slot;
}
} // namespace sgn::retail::contact::effects
