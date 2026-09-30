#include "sgn/track_contact.hpp"
#include "sgn/vehicle_math.hpp"
#include "sgn/source_pmd_model.hpp"
#include <array>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail::contact
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
double dot_zyx(Vec3 a, Vec3 b)
{
    return (double(a.z) * b.z + double(a.y) * b.y) + double(a.x) * b.x;
}
Vec3 normalize_retail(Vec3 v)
{ // 0044ED40/0044EDA0, zero vector is preserved.
    const double squared = (double(v.x) * v.x + double(v.y) * v.y) + double(v.z) * v.z;
    if (squared == 0)
        return {};
    double length = std::sqrt(squared);
    double scale = 1 / length;
    return {f32(v.x * scale), f32(v.y * scale), f32(v.z * scale)};
}
struct Input
{
    std::span<const std::uint8_t> bytes;
    void check(std::size_t at, std::size_t n) const
    {
        if (at > bytes.size() || n > bytes.size() - at)
            throw FormatError("truncated track collision data");
    }
    std::uint32_t u32(std::size_t at) const
    {
        check(at, 4);
        return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) | (std::uint32_t(bytes[at + 2]) << 16) |
               (std::uint32_t(bytes[at + 3]) << 24);
    }
    std::uint16_t u16(std::size_t at) const
    {
        check(at, 2);
        return std::uint16_t(bytes[at]) | static_cast<std::uint16_t>(std::uint16_t(bytes[at + 1]) << 8);
    }
    float scalar(std::size_t at) const
    {
        float x = std::bit_cast<float>(u32(at));
        if (!std::isfinite(x))
            throw FormatError("nonfinite track collision coordinate");
        return x;
    }
    Vec3 vec(std::size_t at) const
    {
        return {scalar(at), scalar(at + 4), scalar(at + 8)};
    }
};
} // namespace
void pack_pose(const Pose &pose, ContactBody &body)
{
    body.position = pose.position;
    body.velocity = pose.velocity;
    body.orientation = pose.orientation;
    body.incremental = pose.incremental;
}
void unpack_pose(const ContactBody &body, Pose &pose)
{
    pose.previous_position = pose.position;
    pose.position = body.position;
    pose.velocity = body.velocity;
    pose.orientation = body.orientation;
    pose.incremental = body.incremental;
}
void update_probe_position(const ContactBody &body, ContactProbe &probe)
{
    probe.previous = probe.current;
    const auto &m = body.orientation;
    const auto &p = probe.local;
    const double x = (double(p.z) * m[8] + double(p.y) * m[4]) + double(p.x) * m[0];
    const float y = f32((double(p.z) * m[9] + double(p.y) * m[5]) + double(p.x) * m[1]);
    const float z = f32((double(p.z) * m[10] + double(p.y) * m[6]) + double(p.x) * m[2]);
    probe.current.x = f32(x + body.position.x);
    probe.current.y = f32(double(y) + body.position.y);
    probe.current.z = f32(double(z) + body.position.z);
}
void update_probe_velocity(const ContactBody &body, ContactProbe &probe)
{
    const auto &m = body.incremental;
    const double x = double(probe.current.x) - body.position.x;
    const double extended_z = double(probe.current.z) - body.position.z;
    const float y = f32(double(probe.current.y) - body.position.y), z = f32(extended_z);
    const double tx = (extended_z * m[8] + double(y) * m[4]) + x * m[0];
    const float ty = f32((double(z) * m[9] + double(y) * m[5]) + x * m[1]);
    const float tz = f32((double(z) * m[10] + double(y) * m[6]) + x * m[2]);
    probe.velocity.x = f32((tx - x) + body.velocity.x);
    probe.velocity.y = f32(double(f32(double(ty) - y)) + body.velocity.y);
    probe.velocity.z = f32(double(f32(double(tz) - z)) + body.velocity.z);
}
void apply_material(Material material, std::int32_t track_mode, WheelContact &wheel)
{
    // 004091E0 ordering matters: mode override is before the type-10 remap.
    wheel.grip = f32(double(material.grip) * 0.015625); // float at 0046E3E0
    wheel.rolling = material.rolling;
    wheel.roughness = material.roughness;
    wheel.previous_type = wheel.type;
    wheel.type = material.flags & 15;
    if (wheel.type == 1)
    {
        if (track_mode == 1)
            wheel.type = 3;
        else if (track_mode == 6)
            wheel.type = 4;
    }
    if (wheel.type == 10)
    {
        wheel.type = 1;
        wheel.grip = 4;
    }
    wheel.changed = wheel.previous_type != wheel.type ? 1 : 0;
}
void filter_wheel_load(float contact_load, WheelContact &wheel)
{
    const double difference = std::max(double(wheel.filtered_load) - contact_load, -6000.0);
    wheel.current_load = contact_load;
    wheel.filtered_load = f32(double(wheel.filtered_load) - difference * 0.125);
    wheel.countdown = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(wheel.countdown) - 1u);
    if (wheel.countdown < 0)
        wheel.filtered_load = 0;
}
void reflect_motion(Vec3 n, float restitution, Vec3 &motion)
{
    const double impulse = (2.0 - (1.0 - double(restitution))) * dot_zyx(n, motion);
    motion = {f32(double(motion.x) - f32(n.x * impulse)), f32(double(motion.y) - f32(n.y * impulse)),
              f32(double(motion.z) - n.z * impulse)};
}
void angular_response(const ContactBody &body, const ContactProbe &probe, Vec3 normal, float angle,
                      const Matrix4 &inertia, Matrix4 &accumulated)
{
    auto transformed = vehicle_transpose(body.orientation);
    vehicle_premultiply(transformed, inertia);
    vehicle_premultiply(transformed, body.orientation);
    const double x = double(body.position.x) - probe.current.x, y = double(body.position.y) - probe.current.y,
                 z = double(body.position.z) - probe.current.z;
    VehicleVector radial{f32((z * transformed[8] + y * transformed[4]) + x * transformed[0]),
                         f32((z * transformed[9] + y * transformed[5]) + x * transformed[1]),
                         f32((z * transformed[10] + y * transformed[6]) + x * transformed[2]), 0};
    radial = vehicle_normalize(radial);
    auto lateral = vehicle_normalize(vehicle_cross({normal.x, normal.y, normal.z, 0}, radial));
    auto up = vehicle_normalize(vehicle_cross(lateral, radial));
    const auto basis = vehicle_basis(lateral, up, radial);
    auto rotation = vehicle_identity();
    vehicle_rotate(rotation, 0, f32(double(angle) * 360 * double(0.15915494f)));
    transformed = vehicle_transpose(basis);
    vehicle_premultiply(transformed, rotation);
    // 0044EFD0 has its own addition order for the second and third rows.
    for (unsigned j = 0; j < 3; ++j)
    {
        auto at = j * 4;
        const double a = transformed[at], b = transformed[at + 1], c = transformed[at + 2];
        rotation[at] = f32((double(basis[8]) * c + b * basis[4]) + double(basis[0]) * a);
        rotation[at + 1] = f32((b * basis[5] + double(basis[1]) * a) + c * basis[9]);
        rotation[at + 2] = f32((b * basis[6] + double(basis[2]) * a) + c * basis[10]);
    }
    vehicle_orthogonalize(rotation);
    vehicle_premultiply(accumulated, rotation);
    vehicle_orthogonalize(accumulated);
}
void accumulate_impact(const ContactBody &body, ContactProbe &probe, Vec3 normal, Vec3 &motion,
                       const ImpactParameters &p, ImpactAccumulator &out, bool *audit_rotation_payload_undefined)
{
    if (!(p.gravity > 0 && p.effective_mass > 0 && probe.mass_fraction > 0))
        throw FormatError("invalid contact impulse mass or gravity");
    out.normal_speed = f32(dot_zyx(normal, motion));
    const float mass = f32(double(probe.mass_fraction) * p.effective_mass);
    const double index = std::abs(double(out.normal_speed)) * 128 / (double(p.gravity) * 4);
    float restitution = 0.05f;
    if (index < 128)
        restitution = f32(double(p.restitution_curve[static_cast<unsigned>(index)]) * double(0.05f));
    float length{};
    auto radial = vehicle_normalize({f32(double(body.position.x) - probe.current.x),
                                     f32(double(body.position.y) - probe.current.y),
                                     f32(double(body.position.z) - probe.current.z), 0},
                                    &length);
    double effective = restitution;
    if (length >= 0.00001f)
    {
        const double nx = double(out.normal_speed) * normal.x, nz = double(out.normal_speed) * normal.z;
        const float ny = f32(double(out.normal_speed) * normal.y), stored_z = f32(nz);
        const double along = (nz * radial.z + double(ny) * radial.y) + nx * radial.x;
        const float px = f32(along * radial.x), py = f32(along * radial.y), pz = f32(along * radial.z);
        const double x = nx - px, y = double(ny) - py, z = double(stored_z) - pz;
        const float angle = f32(-((2.0 - (1.0 - restitution)) * std::sqrt((z * z + y * y) + x * x)) / mass);
        angular_response(body, probe, normal, angle, p.inertia, out.rotation_delta);
        if (audit_rotation_payload_undefined)
            *audit_rotation_payload_undefined = true;
        if (index < 128)
            restitution = f32(double(restitution) * p.restitution_curve[static_cast<unsigned>(index)]);
        effective =
            (1.0 - ((double(normal.z) * radial.z + double(normal.y) * radial.y) + double(normal.x) * radial.x)) *
            restitution;
    }
    out.touched = true;
    const double impulse = -((2.0 - (1.0 - effective)) * out.normal_speed) * p.inverse_probe_count;
    out.velocity_delta = {f32(double(out.velocity_delta.x) + f32(impulse * normal.x)),
                          f32(double(out.velocity_delta.y) + f32(impulse * normal.y)),
                          f32(double(out.velocity_delta.z) + impulse * normal.z)};
    if (probe.contact_iterations < 128)
        ++probe.contact_iterations;
    reflect_motion(normal, restitution, motion);
    if (out.collision_count < 8)
        ++out.collision_count;
}
PlaneBand plane_band(Vec3 start, Vec3 end, Vec3 point, Vec3 n, float radius)
{
    auto distance = [&](Vec3 p) {
        return f32(((double(p.z) - point.z) * n.z + (double(p.x) - point.x) * n.x) + (double(p.y) - point.y) * n.y);
    };
    PlaneBand out{distance(end), distance(start), false};
    out.accepted = !(std::abs(double(out.endpoint_distance)) > double(radius) + 0.25 &&
                     std::abs(double(out.start_distance)) > double(radius) + 0.25);
    return out;
}
namespace
{
// Complete collision record ranges, each span starting at its first decoded
// record and holding exactly count*stride bytes. Shared by both producers so
// the record layout, finiteness checks and payload lanes cannot diverge.
struct CollisionSections
{
    std::span<const std::uint8_t> lists, indices, vertices, normals, polygons, materials, nodes;
};
void decode_sections(const CollisionSections &s, TrackGeometry &out)
{
    const Input lists{s.lists}, indices{s.indices}, vertices{s.vertices}, normals{s.normals}, nodes{s.nodes},
        polygons{s.polygons};
    for (std::size_t i = 0; i < s.lists.size() / 2; ++i)
        out.polygon_lists.push_back(lists.u16(2 * i));
    for (std::size_t i = 0; i < s.indices.size() / 2; ++i)
        out.indices.push_back(indices.u16(2 * i));
    for (std::size_t i = 0; i < s.vertices.size() / 16; ++i)
        out.vertices.push_back(vertices.vec(16 * i)); // W is never read
    for (std::size_t i = 0; i < s.normals.size() / 16; ++i)
    {
        out.normals.push_back(normals.vec(16 * i));
        out.normal_payload.push_back(normals.u32(16 * i + 12));
    }
    for (std::size_t i = 0; i < s.polygons.size() / 8; ++i)
    {
        auto at = 8 * i;
        out.polygons.push_back(
            {s.polygons[at], s.polygons[at + 1], polygons.u16(at + 2), polygons.u16(at + 4), polygons.u16(at + 6)});
    }
    for (std::size_t i = 0; i < s.materials.size() / 4; ++i)
    {
        auto at = 4 * i;
        out.materials.push_back({s.materials[at], s.materials[at + 1], s.materials[at + 2], s.materials[at + 3]});
    }
    for (std::size_t i = 0; i < s.nodes.size() / 48; ++i)
    {
        auto at = 48 * i;
        SpatialNode node;
        node.split = nodes.vec(at);
        node.opaque = nodes.u32(at + 12);
        for (unsigned j = 0; j < 8; ++j)
        {
            node.child[j] = nodes.u16(at + 16 + j * 2);
            node.leaf[j] = nodes.u16(at + 32 + j * 2);
        }
        out.nodes.push_back(node);
    }
}
// Validate exactly the nodes reachable from either root. Child references in
// the second tree are still relative to root[0], as 00420F5B..00420F64 shows.
void validate_reachable(const TrackGeometry &out)
{
    std::vector<std::uint8_t> visited(out.nodes.size());
    auto visit = [&](auto &&self, std::uint32_t at) -> void {
        if (at >= out.nodes.size())
            throw FormatError("track collision node out of bounds");
        if (visited[at] == 1)
            throw FormatError("cyclic track collision tree");
        if (visited[at] == 2)
            return;
        visited[at] = 1;
        const auto &node = out.nodes[at];
        for (unsigned j = 0; j < 8; ++j)
        {
            if (node.child[j])
                self(self, out.roots[0] + node.child[j]);
            else
            {
                std::size_t entry = node.leaf[j];
                for (;; ++entry)
                {
                    if (entry >= out.polygon_lists.size())
                        throw FormatError("unterminated track polygon list");
                    auto id = out.polygon_lists[entry];
                    if (id == 0xffff)
                        break;
                    if (id >= out.polygons.size())
                        throw FormatError("track collision polygon index");
                    const auto &p = out.polygons[id];
                    if (p.vertex_count < 3 || p.normal >= out.normals.size() || p.material >= out.materials.size() ||
                        std::size_t(p.first_index) + p.vertex_count > out.indices.size())
                        throw FormatError("track collision polygon fields");
                    for (unsigned k = 0; k < p.vertex_count; ++k)
                        if (out.indices[p.first_index + k] >= out.vertices.size())
                            throw FormatError("track collision vertex index");
                }
            }
        }
        visited[at] = 2;
    };
    visit(visit, out.roots[0]);
    visit(visit, out.roots[1]);
}
// NEW host reader of the actual track allocation. Every read goes through a
// checked SourcePmdByteView field (thread, bank generation, extent, opaque
// admission). Scratch vectors live only for one decode call.
constexpr std::uint32_t live_site = 0x42ed80; // bytes last owned by the 42ED80..42EE45 relocation
struct LiveImage
{
    SourcePmdByteView file;
    std::uint64_t size{};
    void check(std::uint64_t at, std::uint64_t n) const
    {
        if (at > size || n > size - at)
            throw FormatError("truncated track collision data");
    }
    std::uint32_t u32(std::uint64_t at) const
    {
        check(at, 4);
        return file.field<std::uint32_t>(static_cast<std::uint32_t>(at)).value(live_site);
    }
    std::uint8_t u8(std::uint64_t at) const
    {
        check(at, 1);
        return file.field<std::uint8_t>(static_cast<std::uint32_t>(at)).value(live_site);
    }
    std::vector<std::uint8_t> records(std::uint64_t start, std::uint64_t count, std::uint32_t stride) const
    {
        const std::uint64_t bytes = count * stride;
        check(start, bytes);
        std::vector<std::uint8_t> out(static_cast<std::size_t>(bytes));
        std::uint64_t at = 0;
        for (; bytes - at >= 16; at += 16)
        {
            const auto v =
                file.field<std::array<std::uint32_t, 4>>(static_cast<std::uint32_t>(start + at)).value(live_site);
            std::memcpy(out.data() + at, v.data(), 16);
        }
        for (; bytes - at >= 4; at += 4)
        {
            const auto v = u32(start + at);
            std::memcpy(out.data() + at, &v, 4);
        }
        for (; at < bytes; ++at)
            out[static_cast<std::size_t>(at)] = u8(start + at);
        return out;
    }
    // Vertex W lanes are not read (no admission of an opaque/undefined W is
    // needed); their scratch bytes stay zero and decode_sections never reads them.
    std::vector<std::uint8_t> vertex_records(std::uint64_t start, std::uint64_t count) const
    {
        check(start, count * 16);
        std::vector<std::uint8_t> out(static_cast<std::size_t>(count * 16));
        for (std::uint64_t i = 0; i < count; ++i)
            for (unsigned lane = 0; lane < 3; ++lane)
            {
                const auto v = u32(start + 16 * i + 4 * lane);
                std::memcpy(out.data() + 16 * i + 4 * lane, &v, 4);
            }
        return out;
    }
};
} // namespace
TrackGeometry decode_track_collision(std::span<const std::uint8_t> raw)
{
    Input in{raw};
    in.check(0, 328);
    if (std::string_view(reinterpret_cast<const char *>(raw.data()), 9) != "PMD V1.83")
        throw FormatError("track collision requires PMD V1.83");
    if (in.u32(0x18) != 1)
        throw FormatError("PMD is not a track");
    const std::size_t descriptor = 328ull + in.u32(0xa0);
    in.check(descriptor, 40);
    // Bounds come from the next authored section, including 0xA0 descriptor.
    auto section = [&](std::size_t header, std::size_t index, std::size_t stride) {
        const std::size_t base = 328ull + in.u32(header);
        std::size_t end = raw.size();
        for (std::size_t h = 0x20; h <= 0xa0; h += 4)
        {
            auto p = 328ull + in.u32(h);
            if (p > base)
                end = std::min(end, p);
        }
        const std::size_t start = base + index * stride;
        if (base > raw.size() || start > end)
            throw FormatError("track collision section offset");
        return std::pair{start, (end - start) / stride};
    };
    TrackGeometry out;
    auto [lists, nlists] = section(0x90, in.u32(descriptor), 2);
    auto [indices, nindices] = section(0x8c, in.u32(descriptor + 4), 2);
    auto [vertices, nvertices] = section(0x2c, in.u32(descriptor + 8), 16);
    auto [normals, nnormals] = section(0x20, in.u32(descriptor + 12), 16);
    auto [polygons, npolygons] = section(0x98, in.u32(descriptor + 16), 8);
    auto [materials, nmaterials] = section(0x94, in.u32(descriptor + 20), 4);
    auto [nodes, nnodes] = section(0x9c, 0, 48);
    if (!nlists || !nindices || !nvertices || !nnormals || !npolygons || !nmaterials || !nnodes)
        throw FormatError("empty track collision section");
    decode_sections({raw.subspan(lists, nlists * 2), raw.subspan(indices, nindices * 2),
                     raw.subspan(vertices, nvertices * 16), raw.subspan(normals, nnormals * 16),
                     raw.subspan(polygons, npolygons * 8), raw.subspan(materials, nmaterials * 4),
                     raw.subspan(nodes, nnodes * 48)},
                    out);
    for (unsigned i = 0; i < 2; ++i)
    {
        out.radius_threshold[i] = in.scalar(descriptor + 24 + i * 8);
        out.roots[i] = in.u32(descriptor + 28 + i * 8);
        if (out.roots[i] >= out.nodes.size())
            throw FormatError("track collision root out of bounds");
    }
    validate_reachable(out);
    return out;
}
TrackGeometry decode_track_collision(const SourcePmdTrackView &track)
{
    // The model view spans the whole actual allocation (SourcePmdModel::view,
    // offset 0), whose size is the complete file size requested at 42EFE0.
    const auto file = track.model().bytes();
    if (file.location().offset != 0)
        throw FormatError("live track collision view is not the whole PMD allocation");
    const LiveImage in{file, file.size()};
    in.check(0, 328);
    constexpr std::string_view version = "PMD V1.83";
    for (std::uint32_t i = 0; i < version.size(); ++i)
        if (in.u8(i) != static_cast<std::uint8_t>(version[i]))
            throw FormatError("track collision requires PMD V1.83");
    if (in.u32(0x18) != 1)
        throw FormatError("PMD is not a track");
    const std::uint64_t descriptor = 328ull + in.u32(0xa0);
    in.check(descriptor, 40);
    // 42ED87..42EDA1: [624480]=file+148h+[file+A0h]; the checked accessor
    // must admit exactly that record (it also requires the 40-byte extent).
    if (track.collision_descriptor().location().offset != descriptor)
        throw FormatError("live track collision descriptor differs from header A0");
    // Header words 20h..A0h are never relocated (42EFB0 rewrites only the
    // model/collision/spline descriptors, part records, names and bounds), so
    // the file decoder's extent rule is evaluated on the same values here.
    std::array<std::uint64_t, 33> starts{};
    for (std::uint32_t i = 0; i < starts.size(); ++i)
        starts[i] = 328ull + in.u32(0x20 + 4 * i);
    auto section = [&](std::uint32_t header, std::uint64_t start, std::uint32_t stride) {
        const std::uint64_t base = starts[(header - 0x20) / 4];
        std::uint64_t end = in.size;
        for (const auto p : starts)
            if (p > base)
                end = std::min(end, p);
        // 42EDA7..42EE3E stored start=file+148h+[header]+index*stride; the
        // accessor already checked it against its section. Recover the same
        // index relation and apply the file decoder's own bound.
        if (base > in.size || start < base || (start - base) % stride || start > end)
            throw FormatError("track collision section offset");
        return std::pair{start, (end - start) / stride};
    };
    TrackGeometry out;
    auto [lists, nlists] = section(0x90, track.collision_polygon_lists().location().offset, 2);     // 42EDB6 [+0]
    auto [indices, nindices] = section(0x8c, track.collision_indices().location().offset, 2);       // 42EDC7 [+4]
    auto [vertices, nvertices] = section(0x2c, track.collision_vertices().location().offset, 16);   // 42EDDC [+8]
    auto [normals, nnormals] = section(0x20, track.collision_normals().location().offset, 16);      // 42EDF4 [+C]
    auto [polygons, npolygons] = section(0x98, track.collision_polygons().location().offset, 8);    // 42EE09 [+10]
    auto [materials, nmaterials] = section(0x94, track.collision_materials().location().offset, 4); // 42EE1D [+14]
    auto [nodes, nnodes] = section(0x9c, starts[(0x9c - 0x20) / 4], 48); // node section itself, index 0
    if (!nlists || !nindices || !nvertices || !nnormals || !npolygons || !nmaterials || !nnodes)
        throw FormatError("empty track collision section");
    const auto list_bytes = in.records(lists, nlists, 2), index_bytes = in.records(indices, nindices, 2);
    const auto vertex_bytes = in.vertex_records(vertices, nvertices), normal_bytes = in.records(normals, nnormals, 16);
    const auto polygon_bytes = in.records(polygons, npolygons, 8),
               material_bytes = in.records(materials, nmaterials, 4);
    const auto node_bytes = in.records(nodes, nnodes, 48);
    decode_sections({list_bytes, index_bytes, vertex_bytes, normal_bytes, polygon_bytes, material_bytes, node_bytes},
                    out);
    for (unsigned i = 0; i < 2; ++i)
    {
        // 420F8F/420FB7 compare the radius DWORDs at [624480]+18h/+20h.
        const auto radius = track.collision_tree_radius(i).value(live_site); // descriptor admitted above
        if (!std::isfinite(radius))
            throw FormatError("nonfinite track collision coordinate");
        out.radius_threshold[i] = radius;
        // 420FC6/420FD7 load the relocated roots [624480]+1Ch/+24h (42EE23..42EE3E
        // stored nodes+48*index); 420F5E rebases every child on +1Ch.
        const std::uint64_t root = track.collision_tree(i).location().offset;
        if (root < nodes || (root - nodes) % 48 || (root - nodes) / 48 >= out.nodes.size())
            throw FormatError("track collision root out of bounds");
        out.roots[i] = static_cast<std::uint32_t>((root - nodes) / 48);
    }
    validate_reachable(out);
    return out;
}
std::uint32_t find_polygon_list(const TrackGeometry &track, Vec3 point, float radius)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) || !std::isfinite(radius) ||
        radius < 0)
        throw FormatError("invalid spatial query");
    std::uint32_t node = track.roots[radius <= track.radius_threshold[0] ? 0 : 1];
    for (std::size_t depth = 0; depth < track.nodes.size(); ++depth)
    {
        const auto &n = track.nodes.at(node);
        const unsigned octant =
            (point.x >= n.split.x ? 1u : 0u) | (point.y <= n.split.y ? 4u : 0u) | (point.z <= n.split.z ? 2u : 0u);
        if (!n.child[octant])
            return n.leaf[octant];
        node = track.roots[0] + n.child[octant];
    }
    throw FormatError("cyclic spatial query");
}
bool inside_polygon(const TrackGeometry &track, const Polygon &polygon, Vec3 point)
{
    auto previous = track.vertices.at(track.indices.at(polygon.first_index));
    const auto n = track.normals.at(polygon.normal);
    for (unsigned i = 1; i <= polygon.vertex_count; ++i)
    {
        auto next = track.vertices.at(track.indices.at(polygon.first_index + i % polygon.vertex_count));
        const double x = double(next.x) - previous.x, y = double(next.y) - previous.y, z = double(next.z) - previous.z;
        auto side = normalize_retail({f32(y * n.z - z * n.y), f32(z * n.x - x * n.z), f32(x * n.y - y * n.x)});
        const double distance = ((double(point.z) - previous.z) * side.z + (double(point.y) - previous.y) * side.y) +
                                (double(point.x) - previous.x) * side.x;
        if (distance > 0)
            return false;
        previous = next;
    }
    return true;
}
} // namespace sgn::retail::contact
