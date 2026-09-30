#include "sgn/contact_solver.hpp"
#include "sgn/contact_sphere_cast.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::solver
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
Vector4 vector(VehicleVector v)
{
    return {v.x, v.y, v.z, std::bit_cast<std::uint32_t>(v.w)};
}
VehicleVector vector(Vector4 v)
{
    return {v.x, v.y, v.z, std::bit_cast<float>(v.opaque)};
}
Vector4 velocity(const VehicleState &s)
{
    return {s.velocity.x, s.velocity.y, s.velocity.z, std::bit_cast<std::uint32_t>(s.velocity_w)};
}
void velocity(VehicleState &s, Vector4 v)
{
    s.velocity = {v.x, v.y, v.z};
    s.velocity_w = std::bit_cast<float>(v.opaque);
}
Pose pose(const VehicleState &s)
{
    return {vector(s.position), vector(s.previous_position), velocity(s), s.orientation, s.incremental};
}
void pose(VehicleState &s, const Pose &p)
{
    s.position = vector(p.position);
    s.previous_position = vector(p.previous_position);
    velocity(s, p.velocity);
    s.orientation = p.orientation;
    s.incremental = p.incremental;
}
void import_wheel(const Wheel &source, const WheelSurfaceHistory &history, WheelContact &target)
{
    target.filtered_load = source.load;
    target.countdown = source.contact_countdown;
    target.normal = vector(source.contact_normal);
    target.grip = source.surface_grip;
    target.roughness = source.surface_roughness;
    target.rolling = source.surface_rolling;
    target.current_load = history.current_load;
    target.previous_type = history.previous_type;
    target.type = history.type;
    target.changed = history.changed;
}
void export_wheel(const WheelContact &source, Wheel &target, WheelSurfaceHistory &history)
{
    target.load = source.filtered_load;
    target.contact_countdown = source.countdown;
    target.contact_normal = vector(source.normal);
    target.surface_grip = source.grip;
    target.surface_roughness = source.roughness;
    target.surface_rolling = source.rolling;
    history.current_load = source.current_load;
    history.previous_type = source.previous_type;
    history.type = source.type;
    history.changed = source.changed;
}
void energy(World &world, Vehicle &vehicle, float strength)
{
    auto &s = vehicle.state;
    auto &c = vehicle.contact;
    response::EnergyImpactState state{s.energy_exempt, s.upgrade_active,          s.upgrade_condition,
                                      s.energy,        c.accumulated_energy_loss, c.energy_hold};
    response::consume_impact_energy(state, world.parameters.energy, strength);
    s.energy = state.energy;
    c.accumulated_energy_loss = state.accumulated_loss;
    c.energy_hold = state.hold;
}
void directional(World &world, Vehicle &vehicle, float strength)
{
    auto &s = vehicle.state;
    auto &c = vehicle.contact;
    response::DirectionalImpact state{{s.auxiliary_velocity.x, s.auxiliary_velocity.y, s.auxiliary_velocity.z},
                                      c.direction_magnitude};
    const auto &normal = world.workspace.last_normal;
    response::directional_impact(state, world.parameters.game_mode, {normal.x, normal.y, normal.z}, strength);
    s.auxiliary_velocity.x = state.accumulated.x;
    s.auxiliary_velocity.y = state.accumulated.y;
    s.auxiliary_velocity.z = state.accumulated.z;
    c.direction_magnitude = state.magnitude;
}
void reset_com(Vehicle &vehicle)
{
    vehicle.state.effective_com_height = vehicle.parameters.com_height;
    vehicle.contact.com_reset_ticks = -12;
    vehicle.contact.com_2628 = 0;
    vehicle.contact.com_262c = 0;
}
void audit_particle(World &world, bool position, bool velocity_payload)
{
    if (!world.audit || world.particles.throttle > 0)
        return;
    const auto cursor = world.particles.cursor < 32 ? world.particles.cursor : 0;
    world.audit->particle_position[cursor] = position;
    world.audit->particle_velocity[cursor] = velocity_payload;
}
} // namespace
void initialize(State &state, std::uint32_t selector)
{
    state.body.position.x = 0;
    state.body.position.y = 1000;
    state.body.position.z = -10000;
    state.body.mass = 1000;
    state.body.orientation = vehicle_identity();
    state.body.incremental = vehicle_identity();
    state.matrix_7338 = vehicle_identity();
    state.body.velocity.x = 0;
    state.body.velocity.y = 0;
    state.body.velocity.z = 0;
    response::initialize_rig(selector, state.body.position, state.rig);
}
void place(Vehicle &vehicle, Vector4 position)
{
    auto p = pose(vehicle.state);
    response::place_rig(position, p, vehicle.contact.body, vehicle.contact.rig);
    pose(vehicle.state, p);
}
VehicleSupport support(const State &state)
{
    VehicleSupport result;
    result.body_position = vector(state.body.position);
    for (unsigned i = 0; i < 4; ++i)
    {
        result.local[i] = vector(state.rig.probes[i].local);
        result.current[i] = vector(state.rig.probes[i].current);
    }
    return result;
}
void collide_pair(World &world, unsigned first, unsigned second)
{
    if (first >= 6 || second >= 6)
        throw FormatError("vehicle contact pair index");
    auto &a = world.vehicles[first];
    auto &b = world.vehicles[second];
    auto va = velocity(a.state), vb = velocity(b.state);
    const auto collision = effects::apply_pair_impulse(a.contact.body, b.contact.body, va, vb);
    velocity(a.state, va);
    velocity(b.state, vb);
    if (world.audit)
    {
        world.audit->vehicles[first].velocity = world.audit->vehicles[first].body_velocity;
        world.audit->vehicles[second].velocity = world.audit->vehicles[second].body_velocity;
    }
    const auto &n = collision.direction;
    if (std::abs(double(collision.normal_speed)) > 5.0)
    {
        const float x = f32(double(n.x) * 150.0), y = f32(double(n.y) * 150.0), z = f32(double(n.z) * 150.0);
        // The original temporary position's fourth word is unwritten. Native
        // policy retains the source position payload; xyz remains exact.
        const Vector4 pa{f32(double(a.state.previous_position.x) - x), f32(double(a.state.previous_position.y) - y),
                         f32(double(a.state.previous_position.z) - z),
                         std::bit_cast<std::uint32_t>(a.state.previous_position.w)};
        audit_particle(world, true, world.audit ? world.audit->vehicles[first].velocity : false);
        effects::emit_impact(world.particles, world.random, pa, va, collision.normal_speed);
        const Vector4 pb{f32(double(x) + b.state.previous_position.x), f32(double(y) + b.state.previous_position.y),
                         f32(double(z) + b.state.previous_position.z),
                         std::bit_cast<std::uint32_t>(b.state.previous_position.w)};
        audit_particle(world, true, world.audit ? world.audit->vehicles[second].velocity : false);
        effects::emit_impact(world.particles, world.random, pb, vb, collision.normal_speed);
    }
    a.contact.pair_shock = f32((std::abs(double(collision.normal_speed) * 8.0) + a.contact.pair_shock) * 0.5);
    response::add_impact_count(a.contact.impact_count, world.parameters.game_mode, collision.normal_speed);
    energy(world, a, collision.normal_speed);
    // Retail intentionally reads the previous global contact normal speed.
    if (world.workspace.impact.normal_speed < -double(world.parameters.gravity))
        directional(world, a, collision.normal_speed);
    a.contact.pair_counters.current[second] = 8;
    b.contact.pair_counters.current[first] = 8;
    reset_com(a);
    a.contact.pair_effect_ticks = 60;
}
void impact(World &world, unsigned vehicle_index, unsigned probe_index, Vector4 normal, Material material, Vec3 &motion)
{
    if (vehicle_index >= 6)
        throw FormatError("impact vehicle index");
    auto &vehicle = world.vehicles[vehicle_index];
    auto &c = vehicle.contact;
    auto &s = vehicle.state;
    auto &work = world.workspace;
    if (probe_index >= c.rig.count || probe_index >= 6)
        throw FormatError("impact probe index");
    auto &probe = c.rig.probes[probe_index];
    work.last_normal = normal;
    ImpactParameters p{world.parameters.restitution, world.parameters.gravity, work.inverse_probe_count,
                       s.effective_mass, world.parameters.inertia};
    work.impact.collision_count = std::bit_cast<std::uint32_t>(s.contact_counter);
    accumulate_impact(c.body, probe, {normal.x, normal.y, normal.z}, motion, p, work.impact,
                      world.audit ? &world.audit->rotation_delta : nullptr);
    s.contact_counter = std::bit_cast<std::int32_t>(work.impact.collision_count);
    auto &r = work.response;
    r.base_com_height = vehicle.parameters.com_height;
    r.effective_com_height = s.effective_com_height;
    r.reset_ticks = c.com_reset_ticks;
    r.reset_2628 = c.com_2628;
    r.reset_262c = c.com_262c;
    r.shock = c.shock;
    r.wheel_touched = c.wheel_touched;
    WheelContact wheel{};
    if (probe_index < 4)
        import_wheel(s.wheels[probe_index], c.wheels[probe_index], wheel);
    response::finish_impact_physical(c.body, probe, probe_index, normal,
                                     {c.orientation_snapshot[4], c.orientation_snapshot[5], c.orientation_snapshot[6]},
                                     f32(double(probe.mass_fraction) * s.effective_mass), material,
                                     world.parameters.track_mode, work.impact, wheel, r);
    if (probe_index < 4)
        export_wheel(wheel, s.wheels[probe_index], c.wheels[probe_index]);
    if (world.audit && probe_index < 4 && r.alignment > std::bit_cast<float>(std::uint32_t{0x3f3504f4}))
        world.audit->vehicles[vehicle_index].wheel_normal[probe_index] = world.audit->normal;
    s.effective_com_height = r.effective_com_height;
    c.com_reset_ticks = r.reset_ticks;
    c.com_2628 = r.reset_2628;
    c.com_262c = r.reset_262c;
    c.shock = r.shock;
    c.wheel_touched = r.wheel_touched;
    response::add_impact_count(c.impact_count, world.parameters.game_mode, r.effect_strength);
    energy(world, vehicle, r.effect_strength);
    surface::process_material(s, vehicle.race, c.surface, world.controls, vehicle.id, material, r.alignment,
                              world.parameters.race_mode, world.inverted);
    if (work.impact.normal_speed < -double(world.parameters.gravity))
        directional(world, vehicle, work.impact.normal_speed);
}
int step(World &world, unsigned index)
{
    if (index >= 6)
        throw FormatError("contact step vehicle index");
    auto &vehicle = world.vehicles[index];
    auto &c = vehicle.contact;
    auto &s = vehicle.state;
    auto &work = world.workspace;
    if (c.rig.count < 4 || c.rig.count > 6)
        throw FormatError("retail contact requires 4 to 6 probes");
    if (vehicle.race.collision_hold == 0)
    {
        std::array<effects::PairVehicle, 6> pairs{};
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto &v = world.vehicles[i];
            pairs[i].active = v.race.active;
            pairs[i].cooldown = v.race.collision_hold;
            pairs[i].previous_position = vector(v.state.previous_position);
        }
        const bool hit = effects::find_contact_partner(pairs, index, world.parameters.race_mode, work.selected_partner);
        for (unsigned i = 0; i < 6; ++i)
            world.vehicles[i].race.collision_hold = pairs[i].cooldown;
        if (hit)
            collide_pair(world, index, *work.selected_partner);
    }
    effects::update_pair_counters(c.pair_counters);
    auto p = pose(s);
    pack_pose(p, c.body);
    auto *audit = world.audit ? &world.audit->vehicles[index] : nullptr;
    if (audit)
    {
        audit->body_position = audit->position;
        audit->body_velocity = audit->velocity;
        audit->body_orientation_first = audit->orientation_first;
        audit->body_incremental_first = audit->incremental_first;
        world.audit->rotation_delta = false;
    }
    work.impact.rotation_delta = vehicle_identity();
    work.impact.velocity_delta = {};
    work.impact.touched = false;
    work.inverse_probe_count = f32(1.0 / c.rig.count);
    c.body.mass = s.effective_mass;
    c.body.previous_position = c.body.position;
    if (audit)
        audit->body_previous_position = audit->body_position;
    c.body.position.x = f32(double(c.body.position.x) + c.body.velocity.x);
    c.body.position.y = f32(double(c.body.position.y) + c.body.velocity.y);
    c.body.position.z = f32(double(c.body.position.z) + c.body.velocity.z);
    vehicle_premultiply(c.body.orientation, c.body.incremental);
    vehicle_orthogonalize(c.body.orientation);
    if (audit)
    {
        audit->body_orientation_first = true;
        audit->snapshot_first = true;
    }
    for (unsigned i = 0; i < 12; ++i)
        c.orientation_snapshot[i] = c.body.orientation[i];
    Vec3 sum{};
    for (unsigned i = 0; i < c.rig.count; ++i)
    {
        auto &probe = c.rig.probes[i];
        update_probe_position(c.body, probe);
        update_probe_velocity(c.body, probe);
        ContactCastPayloadAudit local_query_audit;
        auto &query_audit = world.cast_audit ? *world.cast_audit : local_query_audit;
        if (audit)
        {
            auto &payload = audit->probes[i];
            payload.previous = payload.current;
            payload.current = true;
            payload.velocity = true;
            query_audit.start = payload.previous;
        }
        auto &query = world.cast;
        query.start = probe.previous;
        query.radius = probe.radius;
        query.radius_squared = f32(double(probe.radius) * probe.radius);
        query.remaining = 1;
        work.query_mass = c.body.mass; // 00409D26, shared query scalar; never reciprocal or a per-car alias
        const double motion_z = double(probe.current.z) - probe.previous.z;
        work.query_motion = {f32(double(probe.current.x) - probe.previous.x),
                             f32(double(probe.current.y) - probe.previous.y), f32(motion_z)};
        auto &motion = work.query_motion;
        query.travel_limit =
            f32(std::sqrt((motion_z * motion.z + double(motion.y) * motion.y) + double(motion.x) * motion.x));
        work.response.body_touched = 0;
        work.response.contact_load = 0;
        unsigned attempts = 0;
        for (;;)
        {
            const auto hit =
                cast_track_sphere(world.geometry, work, query, &query_audit, world.internals_audit, world.audit);
            if (hit == 1 && query.hit_flag == 1)
            {
                const auto &polygon = world.geometry.polygons.at(query.polygon.value());
                impact(world, index, i, work.last_normal, world.geometry.materials.at(polygon.material), motion);
            }
            else if (hit == 0 && attempts == 0)
                probe.contact_iterations = 0;
            ++attempts;
            if (attempts > 64)
            {
                vehicle.race.respawn_requested = 1;
                return 1;
            }
            if (hit != 1)
                break;
        }
        if (i < 4)
        {
            WheelContact wheel{};
            import_wheel(s.wheels[i], c.wheels[i], wheel);
            filter_wheel_load(work.response.contact_load, wheel);
            export_wheel(wheel, s.wheels[i], c.wheels[i]);
        }
        if (work.response.body_touched == 1)
        {
            if (query.travel_limit > 24.0f &&
                !(double(world.parameters.gravity) * work.inverse_probe_count * -0.25 < work.impact.normal_speed))
            {
                audit_particle(world, audit ? audit->probes[i].current : false,
                               audit ? audit->probes[i].velocity : false);
                effects::emit_impact(world.particles, world.random, probe.current, probe.velocity,
                                     work.impact.normal_speed);
            }
            if (work.impact.normal_speed < 0)
            {
                c.shock_hold = 16;
                c.shock = f32((double(c.shock) - work.impact.normal_speed) * 0.5);
            }
        }
        probe.current.x = query.end.x;
        probe.current.y = query.end.y;
        probe.current.z = query.end.z;
        if (audit)
            audit->probes[i].current = query_audit.end;
        // Query fourth words may originate from undefined retail temporaries.
        // Preserve the probe's opaque lane under the established native policy.
        sum.x = f32(double(query.end.x) + sum.x);
        sum.y = f32(double(query.end.y) + sum.y);
        sum.z = f32(double(query.end.z) + sum.z);
    }
    if (work.impact.touched)
    {
        c.body.position = {f32(double(work.inverse_probe_count) * sum.x), f32(double(work.inverse_probe_count) * sum.y),
                           f32(double(work.inverse_probe_count) * sum.z), work.average_payload};
        if (audit)
            audit->body_position = false; // authored global 0049A198 is always compared
    }
    const auto &delta = work.impact.velocity_delta;
    if (std::sqrt((double(delta.z) * delta.z + double(delta.y) * delta.y) + double(delta.x) * delta.x) > 250)
        vehicle.race.respawn_requested = 1;
    c.body.velocity.x = f32(double(delta.x) + c.body.velocity.x);
    c.body.velocity.y = f32(double(delta.y) + c.body.velocity.y);
    c.body.velocity.z = f32(double(delta.z) + c.body.velocity.z);
    vehicle_premultiply(c.body.incremental, work.impact.rotation_delta);
    const auto wheels = std::span<const ContactProbe, 4>(c.rig.probes.data(), 4);
    response::damp_incremental(c.body.incremental, wheels);
    response::settle_incremental(c.body.incremental, wheels, audit ? &audit->body_incremental_first : nullptr);
    response::limit_incremental(c.body.incremental);
    unpack_pose(c.body, p);
    pose(s, p);
    if (audit)
    {
        audit->previous_position = audit->position;
        audit->position = audit->body_position;
        audit->velocity = audit->body_velocity;
        audit->orientation_first = audit->body_orientation_first;
        audit->incremental_first = audit->body_incremental_first;
    }
    return 0;
}
} // namespace sgn::retail::contact::solver
