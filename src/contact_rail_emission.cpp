#include "sgn/contact_rail.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
RailEmission emit_rail_particles(contact::effects::ImpactParticles &particles, VehicleRandom &random,
                                 const VehicleState &car, const VehicleRailState &rail)
{
    using contact::Vector4;
    const auto f = [](std::uint32_t bits) { return std::bit_cast<float>(bits); };
    const double noise = random.signed_value(f(0x42a6aaab));
    Vector4 velocity{vehicle_f32(noise * car.basis_x.x), vehicle_f32(noise * car.basis_x.y),
                     vehicle_f32(noise * car.basis_x.z), 0};
    velocity.x = vehicle_f32(double(velocity.x) + car.velocity.x);
    velocity.y = vehicle_f32(double(velocity.y) + car.velocity.y);
    velocity.z = vehicle_f32(double(velocity.z) + car.velocity.z);
    const Vector4 position{car.position.x, car.position.y, car.position.z,
                           std::bit_cast<std::uint32_t>(car.position.w)};
    RailEmission result;
    result.primary = contact::effects::emit_refuel_particle(particles, random, position, velocity,
                                                            {car.basis_x.x, car.basis_x.y, car.basis_x.z});
    double side = random.signed_value(f(0x40d55555));
    side += side < 0 ? -double(f(0x40d55555)) : double(f(0x40d55555));
    velocity = {vehicle_f32(side * car.basis_x.x), vehicle_f32(side * car.basis_x.y), vehicle_f32(side * car.basis_x.z),
                0};
    const double rise = std::abs(random.signed_value(f(0x40d55555)));
    const float x = vehicle_f32(rise * car.up.x), y = vehicle_f32(rise * car.up.y);
    velocity.x = vehicle_f32(double(x) + velocity.x);
    velocity.y = vehicle_f32(double(y) + velocity.y);
    velocity.z = vehicle_f32(rise * car.up.z + velocity.z);
    velocity.x = vehicle_f32(double(velocity.x) + car.velocity.x);
    velocity.y = vehicle_f32(double(velocity.y) + car.velocity.y);
    velocity.z = vehicle_f32(double(velocity.z) + car.velocity.z);
    if (particles.throttle == 2)
        velocity.opaque = particles.particles[particles.cursor >= 32 ? 0 : particles.cursor].velocity.opaque;
    result.anchor = contact::effects::emit_refuel_anchor_particle(particles, rail.effect_position, velocity);
    return result;
}
} // namespace sgn::retail
