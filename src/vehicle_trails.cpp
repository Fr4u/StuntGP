#include "sgn/vehicle_trail_append.hpp"
#include "vehicle_speed_trail_provider.hpp"
#include "vehicle_pair_trail_provider.hpp"
#include "vehicle_launch_provider.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
void basis(const VehicleTrailModel &model, VehicleMatrix &out)
{ // full 00413b60 + math callees
    auto x = vehicle_cross(model.base_forward, model.base_up);
    auto y = vehicle_cross(x, model.base_forward); // source uses X before normalizing it
    x = vehicle_normalize(x);
    y = vehicle_normalize(y);
    x.w = out[3];
    y.w = out[7]; // source-unwritten stack W policy
    out = vehicle_basis(x, y, model.base_forward);
}
bool project(VehicleVector position, const VehicleTrailProjection &p, VehicleVector &out)
{ // full 0041e410
    const auto &m = p.world_view;
    const double x = ((double(position.y) * m[4] + double(position.z) * m[8]) + double(m[0]) * position.x) + m[12];
    const float y =
        vehicle_f32(((double(position.y) * m[5] + double(position.z) * m[9]) + double(m[1]) * position.x) + m[13]);
    const double depth = ((double(position.y) * m[6] + double(position.z) * m[10]) + double(m[2]) * position.x) + m[14];
    const float stored_depth = vehicle_f32(depth);
    if (depth < p.clip_near)
        return false; // comparison precedes reloading the float depth
    const double clip_x = x * p.projection_x;
    const float clip_y = vehicle_f32(double(p.projection_y) * y);
    const float inverse = vehicle_f32(1.0 / (double(stored_depth) + 1.0));
    out.x = vehicle_f32((double(p.scale_x) * inverse) * clip_x + p.center_x);
    out.y = vehicle_f32((double(p.scale_y) * inverse) * clip_y + p.center_y);
    out.z = vehicle_f32(double(inverse) * 32.0);
    return true;
}
template <class Binding, class GetBasis>
void append(Binding b, VehicleVector point, std::span<VehicleVector> ring, std::uint32_t &cursor, std::uint32_t &count,
            VehicleTrailParameters parameters, GetBasis get_basis)
{ // full 0041e510
    // A separate latest-position slot lives at [capacity], beyond the cyclic
    // indices. The actual source profiles use capacities 16, 5 and 9.
    if (parameters.capacity == 0 || parameters.capacity >= ring.size())
        throw std::domain_error("trail capacity has no bounded latest-position slot");
    basis(get_basis(), b.model_basis);
    const auto &m = b.model_basis;
    const auto &s = b.vehicle;
    const float rx = vehicle_f32((double(m[8]) * point.z + double(m[4]) * point.y) + double(m[0]) * point.x);
    const float ry = vehicle_f32((double(m[9]) * point.z + double(m[5]) * point.y) + double(m[1]) * point.x);
    const float rz = vehicle_f32((double(m[10]) * point.z + double(m[6]) * point.y) + double(m[2]) * point.x);
    const double side = -double(rx), lift = (70.0 - double(ry)) - s.body_heave;
    const float side_x = vehicle_f32(side * s.basis_x.x), side_y = vehicle_f32(side * s.basis_x.y);
    const double side_z = side * s.basis_x.z;
    const float up_x = vehicle_f32(lift * s.up.x), up_y = vehicle_f32(lift * s.up.y);
    const double up_z = lift * s.up.z;
    const float forward_x = vehicle_f32(double(rz) * s.basis_z.x), forward_y = vehicle_f32(double(rz) * s.basis_z.y);
    const float forward_z = vehicle_f32(double(rz) * s.basis_z.z);
    const float xy_x = vehicle_f32(double(side_x) + up_x), xy_y = vehicle_f32(double(side_y) + up_y);
    const float total_x = vehicle_f32(double(forward_x) + xy_x), total_y = vehicle_f32(double(forward_y) + xy_y);
    const double total_z = (side_z + up_z) + forward_z;
    const VehicleVector world{vehicle_f32(double(s.position.x) - total_x), vehicle_f32(double(s.position.y) - total_y),
                              vehicle_f32(double(s.position.z) - total_z), 0};
    if (count < parameters.capacity)
        ++count;
    cursor = (cursor + 1u) % parameters.capacity; // unsigned INC wraps before DIV
    auto write = [&](VehicleVector &target) { target = {world.x, world.y, world.z, target.w}; };
    write(ring[parameters.capacity]);
    write(ring[cursor]); // undefined source W preserved separately at each destination
    if (b.write_audit && b.write_audit->retained_word)
    {
        auto &audit = *b.write_audit;
        audit.retained_word(audit.context, b.model_basis[3], VehicleTrailOpaqueLane::basis_x);
        audit.retained_word(audit.context, b.model_basis[7], VehicleTrailOpaqueLane::basis_y);
        audit.retained_word(audit.context, ring[parameters.capacity].w, VehicleTrailOpaqueLane::latest);
        audit.retained_word(audit.context, ring[cursor].w, VehicleTrailOpaqueLane::cyclic);
    }
    if (parameters.project_to_screen)
        project(world, b.projection, ring[cursor]);
}
void append(VehicleTrailBindings b, VehicleVector point, std::span<VehicleVector> ring, std::uint32_t &cursor,
            std::uint32_t &count, VehicleTrailParameters parameters)
{
    append(b, point, ring, cursor, count, parameters, [&]() -> const VehicleTrailModel & { return b.model; });
}
struct SnapshotSpeedProvider
{
    const VehicleTrailModel &model;
    float position_z() const
    {
        return model.part23_position_z;
    }
    const VehicleTrailModel &basis_inputs() const
    {
        return model;
    }
};
template <class Binding, class Provider> void speed_trail(Binding b, Provider provider)
{
    if (b.source_tick < 20)
        return;
    auto &counters = b.vehicle.camera_effects.values;
    if (double(b.vehicle.speed) - std::bit_cast<float>(std::uint32_t{0x42855555}) < 0.0)
    {
        counters[2] = 0;
        counters[3] = 0;
        return;
    }
    append(b, {0, b.parameters.height, provider.position_z(), 0}, b.storage.speed, counters[2], counters[3],
           b.settings.speed, [&]() { return provider.basis_inputs(); });
}
template <class Binding, class Provider> void pair_a(Binding b, Provider provider)
{
    if (b.source_tick < 60 || b.inputs.field_28a0 == 0)
        return;
    for (unsigned i = 0; i < 2; ++i)
        append(b, b.inputs.pair_a[i], b.storage.pair_a[i], b.storage.cursor_a[i],
               b.vehicle.camera_effects.values[4 + i], b.settings.pair_a, [&]() { return provider.basis_inputs(); });
}
template <class Binding, class Provider> void pair_b(Binding b, Provider provider)
{
    if (b.source_tick < 60 || b.inputs.field_28a0 == 0)
        return;
    for (unsigned i = 0; i < 2; ++i)
    {
        auto point = b.inputs.pair_b[i];
        point.y = vehicle_f32(double(point.y) - 20.0);
        append(b, point, b.storage.pair_b[i], b.storage.cursor_b[i], b.vehicle.camera_effects.values[6 + i],
               b.settings.pair_b, [&]() { return provider.basis_inputs(); });
    }
}
} // namespace
VehicleTrailSettings initial_vehicle_trail_settings() noexcept
{
    return {{16, false}, {5, true}, {9, true}};
}
bool project_vehicle_trail_point(VehicleVector point, const VehicleTrailProjection &projection, VehicleVector &output)
{
    return project(point, projection, output);
}
void append_vehicle_trail_point(VehicleTrailBindings b, VehicleVector point, std::span<VehicleVector> ring,
                                std::uint32_t &cursor, std::uint32_t &count, VehicleTrailParameters parameters)
{
    append(b, point, ring, cursor, count, parameters);
}
void update_vehicle_speed_trail(VehicleTrailBindings b)
{
    speed_trail(b, SnapshotSpeedProvider{b.model});
}
void detail::update_speed_trail(VehicleSpeedTrailBindings b, detail::SpeedTrailProvider provider)
{
    speed_trail(b, provider);
}
void update_vehicle_pair_a_trails(VehicleTrailBindings b)
{
    pair_a(b, SnapshotSpeedProvider{b.model});
}
void update_vehicle_pair_b_trails(VehicleTrailBindings b)
{
    pair_b(b, SnapshotSpeedProvider{b.model});
}
void detail::update_pair_a(VehiclePairTrailBindings b, detail::PairTrailProvider provider)
{
    pair_a(b, provider);
}
void detail::update_pair_b(VehiclePairTrailBindings b, detail::PairTrailProvider provider)
{
    pair_b(b, provider);
}
void detail::append_launch_trail(VehicleLaunchBindings b, VehicleVector p, std::span<VehicleVector> ring,
                                 std::uint32_t &cursor, std::uint32_t &count, detail::LaunchProvider provider)
{
    append(b, p, ring, cursor, count, {7, false}, [&]() { return provider.basis_inputs(); });
}

} // namespace sgn::retail
