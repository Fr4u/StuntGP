#include "sgn/source_impact_particle_update.hpp"
#include "sgn/source_camera_ground_query.hpp"
#include "sgn/race_session.hpp"
#include <bit>
#include <cstddef>
#include <type_traits>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Particle = contact::effects::ImpactParticle;
using Vector = contact::Vector4;
struct ProjectionOwners
{
    const CameraMatrix *world_view;
    const CameraMatrix *projection;
    const ViewportState *viewport;
    const float *near_plane;
};
static_assert(sizeof(void *) == 8 && sizeof(Vector) == 16 && sizeof(Vec3) == 12);
static_assert(offsetof(Vector, opaque) == 12 && sizeof(CameraMatrix) == 64);
static_assert(std::is_standard_layout_v<Particle> && sizeof(Particle) == 0x440);
static_assert(offsetof(Particle, position) == 8 && offsetof(Particle, velocity) == 0x18);
static_assert(offsetof(Particle, decay) == 0x28 && offsetof(Particle, reserved) == 0x30);
static_assert(offsetof(Particle, field_430) == 0x430 && offsetof(Particle, field_434) == 0x434);
static_assert(offsetof(Particle, alpha) == 0x438 && offsetof(Particle, initial_lifetime) == 0x43c);
static_assert(sizeof(ProjectionOwners) == 32 && offsetof(ProjectionOwners, near_plane) == 24);
static_assert(offsetof(ProjectionOwners, world_view) == 0 && offsetof(ProjectionOwners, projection) == 8 &&
              offsetof(ProjectionOwners, viewport) == 16);
static_assert(offsetof(ViewportState, center_x) == 16 && offsetof(ViewportState, center_y) == 20);
static_assert(offsetof(ViewportState, scale_x) == 24 && offsetof(ViewportState, scale_y) == 28);
static_assert(offsetof(VehicleRandom, state) == 0);
} // namespace
// AMD64 leaves consume only borrowed actual owners. x87 intermediates remain
// on the FPU stack until the original FST/FSTP sites; no double return bridge.
extern "C" void sgn_source_impact_move_415f5a(Vector *, const Vector *) noexcept;
extern "C" void sgn_source_impact_gravity_401d70(Vector *, const Vec3 *) noexcept;
extern "C" void sgn_source_impact_decay_415f82(Vector *, const float *) noexcept;
extern "C" void sgn_source_impact_jitter_415fa5(Particle *, W *) noexcept;
extern "C" void sgn_source_impact_length_415ebd(const Vec3 *, const W *, W *, float *) noexcept;
extern "C" void sgn_source_impact_reflect_421c30(Vec3 *, const Vector *) noexcept;
extern "C" W sgn_source_impact_project_41e410(const Vector *, W *, const ProjectionOwners *) noexcept;
namespace
{
W position_word(const Particle &particle, unsigned lane)
{
    switch (lane)
    {
    case 0:
        return std::bit_cast<W>(particle.position.x);
    case 1:
        return std::bit_cast<W>(particle.position.y);
    case 2:
        return std::bit_cast<W>(particle.position.z);
    case 3:
        return particle.position.opaque;
    default:
        throw SourceAllocationError(0x41e710, "source impact position word outside16-byte owner");
    }
}
void store_ring(Particle &particle, W index, unsigned lane, W value, W site)
{
    const auto word = std::uint64_t(index) * 4u + lane;
    if (word >= particle.reserved.size())
        throw SourceAllocationError(site, "reached source impact trail store outside64-point owner");
    particle.reserved[static_cast<std::size_t>(word)] = value;
}
void project_41e410(RaceSessionState &world, const Vector &position, W *output)
{
    // Borrow the source current camera, never a copied CameraVehicle/view.
    // This host pointer-domain admission is reached after both trail copies.
    // The numeric leaf loads its near field only at41E48D and leaves all four
    // output words intact for C0 (including an unordered depth comparison).
    const auto camera = world.controls.current;
    if (camera >= world.controls.controllers.size() || camera >= world.camera_lifecycle.size())
        throw SourceAllocationError(0x41e48d, "source impact projection current camera outside actual owner bank");
    const ProjectionOwners owners{&world.frame_world_view, &world.frame_projection, &world.viewport,
                                  &world.camera_lifecycle[camera].clip_near};
    (void)sgn_source_impact_project_41e410(&position, output, &owners);
}
void append_41e6e0(RaceSessionState &world, Particle &particle)
{
    // 415FE7 resolves this particle's type only after movement/gravity/random.
    // Retain the selected real profile; do not precompute all nine capacities.
    const auto type = std::bit_cast<W>(particle.type);
    if (type >= world.trail_profiles.profiles.size())
        throw SourceAllocationError(0x41e6ee, "reached impact type outside actual nine-profile bank");
    const auto &profile = world.trail_profiles.profiles[type];
    const auto capacity = profile.capacity; // 41E6EE, captured ESI through both copies
    if (particle.field_434 < capacity)
        ++particle.field_434;                  // 41E6F1..F8
    const auto next = particle.field_430 + 1u; // unsigned INC wraps before DIV
    if (capacity == 0)
        throw SourceAllocationError(0x41e6ff, "reached impact trail DIV has zero capacity");
    const auto cursor = next % capacity;
    particle.field_430 = cursor; // 41E709, before the first bounded output store
    // Each original DWORD is fetched at the corresponding store. Ring writes
    // precede the profile+10 reload and a possibly rejected projection.
    store_ring(particle, capacity, 0, position_word(particle, 0), 0x41e714);
    store_ring(particle, capacity, 1, position_word(particle, 1), 0x41e71b);
    store_ring(particle, capacity, 2, position_word(particle, 2), 0x41e721);
    store_ring(particle, capacity, 3, position_word(particle, 3), 0x41e727);
    store_ring(particle, cursor, 0, position_word(particle, 0), 0x41e72e);
    store_ring(particle, cursor, 1, position_word(particle, 1), 0x41e733);
    const auto z = position_word(particle, 2), w = position_word(particle, 3);
    store_ring(particle, cursor, 2, z, 0x41e73c);
    store_ring(particle, cursor, 3, w, 0x41e740);
    if (profile.shape_identity == 0) // actual profile+10, loaded after all eight stores
        project_41e410(world, particle.position, particle.reserved.data() + std::size_t(cursor) * 4u);
}
void collide_415e4f(RaceSessionPlatform &platform, Particle &particle, SourceCameraGroundQuery &query_child)
{
    auto &world = platform.state();
    auto &q = world.contact_cast;
    auto &work = world.contact_workspace;
    work.query_mass = 1.0f;
    q.radius = 25.0f;
    q.radius_squared = 625.0f;
    q.remaining = 1.0f;
    q.start.x = particle.position.x;
    q.start.y = particle.position.y;
    q.start.z = particle.position.z;
    q.start.opaque = particle.position.opaque;
    work.query_motion.x = particle.velocity.x;
    work.query_motion.y = particle.velocity.y;
    work.query_motion.z = particle.velocity.z;
    // The velocity W publication is inside the leaf between the second
    // squared component and FADDP, exactly415ED5..415ED8.
    sgn_source_impact_length_415ebd(&work.query_motion, &particle.velocity.opaque, &q.motion_payload, &q.travel_limit);
    W responses = 0;
    for (;;)
    {
        if (query_child.cast_source_track_particle_sphere_4215b0(platform) != 1)
            break;
        sgn_source_impact_reflect_421c30(&work.query_motion, &work.last_normal);
        const auto previous = responses++;
        if (previous >= 8u)
            break; // 415F0A..10: nine responses, no tenth query
    }
    const auto ex = q.end.x, ey = q.end.y;
    particle.position.x = ex;
    const auto ez = q.end.z;
    particle.position.y = ey;
    const auto ew = q.end.opaque;
    particle.position.z = ez;
    const auto mx = work.query_motion.x;
    particle.position.opaque = ew;
    const auto my = work.query_motion.y;
    particle.velocity.x = mx;
    const auto mz = work.query_motion.z;
    particle.velocity.y = my;
    const auto mw = q.motion_payload;
    particle.velocity.z = mz;
    particle.velocity.opaque = mw;
}
} // namespace
void update_source_impact_particles_415df0(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(0x415df0, "impact update requires actual source platform");
    auto &world = platform.state();
    auto &bank = world.impact_particles;
    auto &draw = world.impact_draw_list;
    if (bank.throttle != 0)
        bank.throttle = std::bit_cast<std::int32_t>(std::bit_cast<W>(bank.throttle) - 1u);
    const auto emitted = bank.emitted; // 415E03, before cursor publication415E08
    draw.count = 0;
    if (emitted == 0)
        return;                          // no particle/profile/camera/geometry access on this path
    SourceCameraGroundQuery query_child; // stateless receiver, no new world or allocation
    for (W slot = 0; slot < bank.particles.size(); ++slot)
    {
        auto &particle = bank.particles[slot];
        const auto lifetime = particle.lifetime;
        if (lifetime <= 0)
            continue;
        particle.lifetime = lifetime - 1;
        if (particle.lifetime <= 0)
            --bank.emitted;
        // Expiry does not skip this tick's movement, trail or draw publication.
        if (bank.rate_selector >= 80u)
            collide_415e4f(platform, particle, query_child);
        else
            sgn_source_impact_move_415f5a(&particle.position, &particle.velocity);
        sgn_source_impact_gravity_401d70(&particle.velocity, &world.gravity.vector);
        sgn_source_impact_decay_415f82(&particle.velocity, &particle.decay);
        if (particle.initial_lifetime != 0)
            sgn_source_impact_jitter_415fa5(&particle, &world.random.state);
        append_41e6e0(world, particle);
        const auto cursor = draw.count; // 416011, only after all append/projection stores
        if (cursor >= draw.slots.size())
            throw SourceAllocationError(0x41601a, "reached impact draw cursor outside32-slot owner");
        draw.slots[cursor] = slot;    // normalized actual particle identity
        draw.count = draw.count + 1u; // 41601C re-read, then416024 publication
    }
}
} // namespace sgn::retail
