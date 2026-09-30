#include "sgn/contact_load_particles.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
using sgn::bit_reinterpret::as_float;
std::uint32_t integer(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("load particle integer domain");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
} // namespace
unsigned emit_load_particle(ImpactParticles &bank, VehicleRandom &random, const VehicleState &car,
                            const Vector4 &position, const Vector4 &velocity, float strength)
{
    if (bank.cursor >= 32)
        bank.cursor = 0;
    const auto index = bank.cursor;
    auto &particle = bank.particles[index];
    particle.position = position;
    particle.type = 4;
    particle.lifetime = vehicle_random_integer(random, 30) + 60;
    particle.decay = as_float(0x3f7d70a4);
    particle.alpha = 1;
    particle.initial_lifetime = particle.lifetime;
    particle.field_430 = 0;
    particle.field_434 = 0;
    particle.scale = vehicle_f32(std::abs(double(strength)) * 0.25 + 0.5);
    const float side = vehicle_f32(random.signed_value(as_float(0x3ed55555)));
    const double rise =
        std::abs(random.signed_value(as_float(0x3f555555)) + (double(car.speed) * as_float(0x3d088889)) * 2.5);
    particle.velocity.x = vehicle_f32(rise * car.orientation[4] + double(side) * car.orientation[0]);
    particle.velocity.y = vehicle_f32(rise * car.orientation[5] + double(side) * car.orientation[1]);
    particle.velocity.z = vehicle_f32(rise * car.orientation[6] + double(side) * car.orientation[2]);
    particle.velocity.x = vehicle_f32(double(particle.velocity.x) + velocity.x);
    particle.velocity.y = vehicle_f32(double(velocity.y) + particle.velocity.y);
    particle.velocity.z = vehicle_f32(double(velocity.z) + particle.velocity.z); // prior W stays untouched
    ++bank.cursor;
    ++bank.emitted;
    return index;
}
std::array<std::optional<unsigned>, 2> emit_load_particles(ImpactParticles &bank, VehicleRandom &random,
                                                           const VehicleState &car, const solver::State &contact,
                                                           VehicleParticleState &state, VehicleAudioState &audio,
                                                           float gravity_magnitude, solver::PayloadAudit *audit)
{
    std::array<std::optional<unsigned>, 2> result;
    if (car.contact_class == 1 || car.speed < 10)
    {
        state.field_761c = 0;
        return result;
    }
    const float threshold = vehicle_f32((double(gravity_magnitude) * car.effective_mass) * 3);
    unsigned loaded = 0;
    double sum = 0;
    for (const auto &wheel : contact.wheels)
    {
        if (wheel.type == 1 && wheel.current_load > threshold)
            ++loaded;
        sum += wheel.current_load;
    }
    if (loaded >= 2)
    {
        const auto increase = std::min(integer(sum * as_float(0x3b000000)), 8u);
        if (std::bit_cast<std::int32_t>(state.field_761c) < 15)
            state.field_761c += increase;
    }
    if (std::bit_cast<std::int32_t>(state.field_761c) <= 0)
        return result;
    --state.field_761c;
    const float strength = vehicle_f32(sum * 0.25);
    const float y = vehicle_f32(double(car.up.y) * -25), z = vehicle_f32(double(car.up.z) * -25);
    Vector4 position{vehicle_f32(double(car.up.x) * -25 + car.position.x), vehicle_f32(double(y) + car.position.y),
                     vehicle_f32(double(z) + car.position.z), 0};
    const Vector4 velocity{car.velocity.x, car.velocity.y, car.velocity.z,
                           std::bit_cast<std::uint32_t>(car.velocity_w)};
    for (unsigned j = 0; j < 2; ++j)
    {
        const auto slot = bank.cursor >= 32 ? 0 : bank.cursor;
        position.opaque = bank.particles[slot].position.opaque;
        result[j] = emit_load_particle(bank, random, car, position, velocity, strength);
        if (audit)
            audit->particle_position[*result[j]] = true;
    }
    audio.effect_pending = 1;
    audio.effect_strength = strength;
    return result;
}
} // namespace sgn::retail::contact::effects
