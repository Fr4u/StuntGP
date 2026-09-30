#include "sgn/impact_particle_lifetime.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
Vec3 xyz(Vector4 value)
{
    return {value.x, value.y, value.z};
}
void set_xyz(Vector4 &target, Vec3 value)
{
    target.x = value.x;
    target.y = value.y;
    target.z = value.z;
}
void append(ImpactParticle &particle, const VehicleTrailProjection &projection, VehicleTrailParameters profile,
            std::array<bool, 64> *audit, bool undefined)
{
    if (particle.field_434 < profile.capacity)
        ++particle.field_434;
    particle.field_430 = (particle.field_430 + 1u) % profile.capacity;
    const auto position = std::bit_cast<std::array<std::uint32_t, 4>>(particle.position);
    for (const auto index : {profile.capacity, particle.field_430})
    {
        for (unsigned lane = 0; lane < 4; ++lane)
            particle.reserved[index * 4 + lane] = position[lane];
        if (audit)
            (*audit)[index] = undefined;
    }
    if (profile.project_to_screen)
    {
        auto output = std::bit_cast<VehicleVector>(position);
        project_vehicle_trail_point(output, projection, output);
        const auto projected = std::bit_cast<std::array<std::uint32_t, 4>>(output);
        for (unsigned lane = 0; lane < 4; ++lane)
            particle.reserved[particle.field_430 * 4 + lane] = projected[lane];
    }
}
} // namespace
ImpactTrailProfiles initial_impact_trail_profiles() noexcept
{
    return {
        {{7, false}, {16, false}, {5, true}, {9, true}, {3, true}, {12, false}, {12, false}, {48, false}, {3, true}}};
}
void update_impact_particles(ImpactParticleLifetimeBindings b)
{
    // Validate the source type before publication/movement can partly mutate a bank.
    if (b.particles.emitted)
        for (const auto &particle : b.particles.particles)
            if (particle.lifetime > 0)
            {
                if (particle.type < 0 || static_cast<unsigned>(particle.type) >= b.profiles.size())
                    throw FormatError("impact particle trail profile outside retail0..8 domain");
                const auto capacity = b.profiles[static_cast<unsigned>(particle.type)].capacity;
                if (capacity == 0 || capacity >= 64)
                    throw FormatError("impact particle trail capacity outside bounded storage");
            }
    auto &bank = b.particles;
    if (bank.throttle != 0)
        bank.throttle = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(bank.throttle) - 1u);
    b.draw_list.count = 0;
    if (bank.emitted == 0)
        return;
    for (unsigned j = 0; j < bank.particles.size(); ++j)
    {
        auto &p = bank.particles[j];
        if (p.lifetime <= 0)
            continue;
        --p.lifetime;
        if (p.lifetime <= 0)
            --bank.emitted;
        auto *audit = b.audit ? &b.audit->query : nullptr;
        if (bank.rate_selector >= 80)
        {
            auto &q = b.query;
            auto &work = b.workspace;
            work.query_mass = 1;
            q.radius = 25;
            q.radius_squared = 625;
            q.remaining = 1;
            q.start = p.position;
            work.query_motion = xyz(p.velocity);
            q.motion_payload = p.velocity.opaque;
            q.travel_limit =
                vehicle_f32(std::sqrt((double(p.velocity.z) * p.velocity.z + double(p.velocity.y) * p.velocity.y) +
                                      double(p.velocity.x) * p.velocity.x));
            if (audit)
            {
                audit->start = b.payload_audit ? b.payload_audit->particle_position[j] : false;
                audit->motion = b.payload_audit ? b.payload_audit->particle_velocity[j] : false;
            }
            for (unsigned attempt = 0; attempt < 9; ++attempt)
            {
                if (cast_track_particle_sphere(b.track, work, q, audit, b.payload_audit) != 1)
                    break;
                reflect_motion(xyz(work.last_normal), 1, work.query_motion);
            }
            p.position = q.end;
            set_xyz(p.velocity, work.query_motion);
            p.velocity.opaque = q.motion_payload;
            if (b.payload_audit && audit)
            {
                b.payload_audit->particle_position[j] = audit->end;
                b.payload_audit->particle_velocity[j] = audit->motion;
            }
        }
        else
        {
            p.position.x = vehicle_f32(double(p.velocity.x) + p.position.x);
            p.position.y = vehicle_f32(double(p.velocity.y) + p.position.y);
            p.position.z = vehicle_f32(double(p.velocity.z) + p.position.z);
        }
        auto velocity = xyz(p.velocity);
        apply_gravity(velocity, b.gravity, 1);
        set_xyz(p.velocity, velocity);
        p.velocity.x = vehicle_f32(double(p.decay) * p.velocity.x);
        p.velocity.y = vehicle_f32(double(p.decay) * p.velocity.y);
        p.velocity.z = vehicle_f32(double(p.decay) * p.velocity.z);
        if (p.initial_lifetime != 0)
        {
            p.velocity.x = vehicle_f32(b.random.signed_value(3) * p.alpha + p.velocity.x);
            p.velocity.y = vehicle_f32(std::abs(b.random.signed_value(2)) * p.alpha + p.velocity.y);
            p.velocity.z = vehicle_f32(b.random.signed_value(3) * p.alpha + p.velocity.z);
        }
        append(p, b.projection, b.profiles[static_cast<unsigned>(p.type)],
               b.audit ? &b.audit->trail_position[j] : nullptr,
               b.payload_audit ? b.payload_audit->particle_position[j] : false);
        b.draw_list.slots[b.draw_list.count++] = j;
    }
}
} // namespace sgn::retail::contact::effects
