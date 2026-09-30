#include "sgn/vehicle_skid_strip.hpp"

namespace sgn::retail
{
namespace
{
void add_xyz(VehicleVector &target, const VehicleVector &a, const VehicleVector &b)
{
    target.x = vehicle_f32(double(a.x) + b.x);
    target.y = vehicle_f32(double(a.y) + b.y);
    target.z = vehicle_f32(double(a.z) + b.z);
}
void advance(VehicleStripHeader &header, VehicleStripGeometry &geometry)
{
    geometry.field_5712f0 = (geometry.field_5712f0 + 1) & 127;
    if (geometry.field_5712f4 < 128)
        ++geometry.field_5712f4;
    header.field_08 = geometry.field_5712f0;
    ++header.field_04;
}
} // namespace
void emit_vehicle_skid_quad(VehicleSkidRing &ring, unsigned index, VehicleStripGeometry &g, Vec3 up)
{
    const auto &segment = ring.segments.at(index);
    const auto first = segment.first_vertex, previous = ring.segments[(index - 1) & 63].first_vertex;
    if (first > 508 || previous > 508 || g.polygon_count >= 128)
        throw std::out_of_range("skid geometry source allocation bounds");
    auto side =
        vehicle_normalize(VehicleVector{vehicle_f32(double(up.z) * segment.step.y - double(segment.step.z) * up.y),
                                        vehicle_f32(double(segment.step.z) * up.x - double(up.z) * segment.step.x),
                                        vehicle_f32(double(up.y) * segment.step.x - double(up.x) * segment.step.y), 0});
    side.x = vehicle_f32(double(side.x) * 25);
    side.y = vehicle_f32(double(side.y) * 25);
    side.z = vehicle_f32(double(side.z) * 25);
    const VehicleVector negative{-side.x, -side.y, -side.z, 0};
    add_xyz(g.vertices[first], side, segment.start);
    add_xyz(g.vertices[first + 1], segment.start, negative);
    add_xyz(g.vertices[first + 2], side, segment.end);
    add_xyz(g.vertices[first + 3], segment.end, negative);
    ring.last_corner = first;
    g.polygons[g.polygon_count].texture = 5;
    // Read previous vertices after all four writes: ring wrap may alias them.
    const double x = double(g.vertices[first].x) - g.vertices[previous + 2].x;
    const double y = double(g.vertices[first].y) - g.vertices[previous + 2].y;
    const double z = double(g.vertices[first].z) - g.vertices[previous + 2].z;
    if ((z * z + y * y) + x * x < 400)
    {
        g.vertices[first] = g.vertices[previous + 2];
        g.vertices[first + 1] = g.vertices[previous + 3];
    }
    else
    {
        g.corners[first].color &= 0x00ffffff;
        g.corners[first + 1].color &= 0x00ffffff;
    }
    if (g.field_5712f4 > 126)
        g.field_5712f4 = 127;
    g.polygon_count = static_cast<std::uint16_t>(g.field_5712f4);
    g.corner_count = g.vertex_count = static_cast<std::uint16_t>(g.field_5712f4 << 2);
}
void update_vehicle_wheel_skid(unsigned wheel, const VehicleState &vehicle,
                               const VehiclePresentationState &presentation, VehicleStripHeaders &headers,
                               VehicleSkidRings &rings, VehicleStripGeometry &geometry, VehicleSkidWriteAudit *audit)
{
    if (audit)
        audit->undefined_writes.at(wheel).reset();
    if (!geometry.field_5712f8)
        return;
    auto &header = headers.items.at(wheel);
    auto &ring = rings.items.at(wheel);
    const auto usage = vehicle.wheels.at(wheel).grip_usage;
    if ((vehicle.speed > 30 && usage < 8.5f) || (vehicle.speed < 30 && usage < 2) || vehicle.contact_class != 0)
    {
        if (header.field_00)
            advance(header, geometry);
        header.field_00 = 0;
        return;
    }
    auto current = presentation.wheel_centers.at(wheel);
    current.x = vehicle_f32(double(current.x) + vehicle_f32(double(vehicle.up.x) * -60));
    current.y = vehicle_f32(double(current.y) + vehicle_f32(double(vehicle.up.y) * -60));
    current.z = vehicle_f32(double(current.z) + vehicle_f32(double(vehicle.up.z) * -60));
    auto index = header.field_04 & 63;
    auto &initial = ring.segments[index];
    if (!header.field_00)
        initial.start = current;
    header.field_00 = 1;
    VehicleVector step{vehicle_f32(double(current.x) - initial.start.x),
                       vehicle_f32(double(current.y) - initial.start.y),
                       vehicle_f32(double(current.z) - initial.start.z), 0};
    float distance{};
    step = vehicle_normalize(step, &distance);
    if (distance > 800)
    {
        header.field_00 = 0;
        return;
    }
    step.x = vehicle_f32(double(step.x) * 200);
    step.y = vehicle_f32(double(step.y) * 200);
    step.z = vehicle_f32(double(step.z) * 200);
    const bool defined_end = distance < 200;
    auto end = initial.end;
    if (defined_end)
        end = current;
    else
        add_xyz(end, initial.start, step);
    initial.end = end;
    if (audit)
    {
        audit->undefined_writes[wheel].set(index * 3 + 1, !defined_end);
        audit->undefined_writes[wheel].set(index * 3 + 2);
    }
    // 41CD30 writes EBP-14 without initializing it; for long segments EBP-24
    // is also unwritten. Native policy retains each destination's opaque W.
    initial.step.x = step.x;
    initial.step.y = step.y;
    initial.step.z = step.z;
    initial.first_vertex = header.field_08 << 2;
    emit_vehicle_skid_quad(ring, index, geometry, vehicle.up);
    if (defined_end)
    {
        const auto corner = ring.last_corner.value();
        geometry.corners[corner + 2].color &= 0x00ffffff;
        geometry.corners[corner + 3].color &= 0x00ffffff;
        return;
    }
    float cumulative = 200;
    for (;;)
    {
        advance(header, geometry);
        index = (index + 1) & 63;
        const auto start = end;
        add_xyz(end, end, step);
        cumulative = vehicle_f32(double(cumulative) + 200);
        auto &segment = ring.segments[index];
        // The last seed uses the previous endpoint, not the newly advanced end.
        segment.start.x = start.x;
        segment.start.y = start.y;
        segment.start.z = start.z;
        if (audit)
            audit->undefined_writes[wheel].set(index * 3);
        if (distance <= cumulative)
            break;
        segment.end.x = end.x;
        segment.end.y = end.y;
        segment.end.z = end.z;
        segment.step.x = step.x;
        segment.step.y = step.y;
        segment.step.z = step.z;
        segment.first_vertex = header.field_08 << 2;
        if (audit)
        {
            audit->undefined_writes[wheel].set(index * 3 + 1);
            audit->undefined_writes[wheel].set(index * 3 + 2);
        }
        emit_vehicle_skid_quad(ring, index, geometry, vehicle.up);
    }
}
void update_vehicle_skid(const VehicleState &vehicle, const VehicleRaceState &race,
                         const VehiclePresentationState &presentation, const ReplayBank &replay, std::uint32_t quality,
                         VehicleStripHeaders &headers, VehicleSkidRings &rings, VehicleStripGeometry &geometry,
                         VehicleSkidWriteAudit *audit)
{
    if (audit)
        *audit = {};
    if (quality < 80 || (race.recording && replay.status(*race.recording) == 1))
        return;
    update_vehicle_wheel_skid(0, vehicle, presentation, headers, rings, geometry, audit);
    update_vehicle_wheel_skid(1, vehicle, presentation, headers, rings, geometry, audit);
}
} // namespace sgn::retail
