#include "sgn/source_camera_ground_query.hpp"
#include "sgn/race_session.hpp"
#include "sgn/contact_geometry_pointer.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using H = std::uint16_t;
using B = std::uint8_t;
using Ref = contact::SourceContactAddress;
using Query = contact::ContactCastState;
using Work = contact::solver::Workspace;
float f32(double v)
{
    return static_cast<float>(v);
}
using sgn::bit_reinterpret::as_float;
bool c0(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
bool c03(double a, double b)
{
    return a <= b || std::isnan(a) || std::isnan(b);
}
Ref plus(Ref p, std::uint64_t delta)
{
    p.offset += delta;
    return p;
}
struct Memory
{
    RaceSessionPlatform &platform;
    template <class T> T read(Ref p, std::uint64_t delta, W site) const
    {
        static_assert(std::endian::native == std::endian::little);
        if (p.offset > std::numeric_limits<W>::max() || delta > std::uint64_t(std::numeric_limits<W>::max()) - p.offset)
            throw SourceAllocationError(site, "contact normalized offset leaves source32-bit domain");
        try
        {
            const auto bytes = platform.source_allocations().bytes(p.allocation);
            const auto at = p.offset + delta;
            if (at > bytes.size() || sizeof(T) > bytes.size() - at)
                throw SourceAllocationError(site, "reached contact geometry field outside live allocation");
            T value;
            std::memcpy(&value, bytes.data() + at, sizeof(T));
            return value;
        }
        catch (const SourceAllocationError &e)
        {
            throw SourceAllocationError(site, e.what());
        }
    }
    Ref collision() const
    {
        const auto p = platform.source_scene_resources().models().source_loading_state().track.collision;
        return {p.allocation, p.offset}; // actual624480, no model-header reconstruction
    }
    Ref pointer(Ref owner, W field, W site) const
    {
        const auto offset = read<W>(owner, field, site);
        return {offset ? owner.allocation : SourceAllocationRef{}, offset};
    }
};
template <class V> void normalize_44eda0(V &v, float &length)
{
    const double z = v.z, y = v.y, x = v.x; // source read order
    const double squared = (x * x + y * y) + z * z;
    if (squared == 0 || std::isnan(squared))
    {
        length = 0;
        v.z = 0;
        v.y = 0;
        v.x = 0;
        return;
    }
    const double root = std::sqrt(squared);
    length = f32(root); // FST retains root
    const double inverse = 1.0 / root;
    v.x = f32(inverse * v.x);
    v.y = f32(inverse * v.y);
    v.z = f32(inverse * v.z);
}
W ceil_word(double value)
{
    if (!std::isfinite(value))
        throw SourceAllocationError(0x462ad6, "source sphere ceil requires recovered finite CRT domain");
    const double integer = std::ceil(value);
    constexpr double limit = 9223372036854775808.0;
    if (integer < -limit || integer >= limit)
        return 0; // 462860 masked indefinite low DWORD
    return static_cast<W>(static_cast<std::int64_t>(integer));
}
void tree_420f00(Memory m, Query &q, Ref node)
{
    if (q.polygon_list != 0)
        return;
    const auto descriptor = m.collision(); // 420F0A capture, before all node reads
    W followed = 0;
    for (;;)
    {
        W octant = 0;
        if (!c0(q.end.x, m.read<float>(node, 0, 0x420f16)))
            octant = 1;
        if (c03(q.end.y, m.read<float>(node, 4, 0x420f2c)))
            octant |= 4;
        if (c03(q.end.z, m.read<float>(node, 8, 0x420f3f)))
            octant |= 2;
        const auto child = m.read<H>(node, 16u + octant * 2u, 0x420f4c);
        if (child == 0)
        {
            q.polygon_list = m.read<H>(node, 32u + octant * 2u, 0x420f6b);
            return;
        }
        // Even a tier1 root follows child indices relative to descriptor+1C.
        const auto base = m.pointer(descriptor, 28, 0x420f5e);
        node = plus(base, std::uint64_t(child) * 48);
        // A fixed base and16-bit nonzero child index admit at most65535 unique
        // child nodes. More traversals prove a cycle, never a source no-hit.
        if (++followed > 65535)
            throw SourceAllocationError(0x420f66, "source collision tree has a reached cycle");
    }
}
void select_420f80(Memory m, Query &q)
{
    const auto radius = q.radius;
    const auto descriptor = m.collision();
    const auto threshold = m.read<float>(descriptor, 24, 0x420f8f);
    q.polygon_list = 0;
    q.internals.tree_tier = 0;
    if (!c03(radius, threshold))
    {
        q.internals.tree_tier = 1;
        tree_420f00(m, q, m.pointer(descriptor, 36, 0x420fd7));
    }
    else
        tree_420f00(m, q, m.pointer(descriptor, 28, 0x420fc6));
}
double plane_projection(Memory m, const contact::Vector4 &p, Ref point, Ref normal, bool start)
{
    const double x = double(p.x) - m.read<float>(point, 0, start ? 0x420aaf : 0x420a81);
    const double y = double(p.y) - m.read<float>(point, 4, start ? 0x420ab7 : 0x420a89);
    const double z = double(p.z) - m.read<float>(point, 8, start ? 0x420ac0 : 0x420a92);
    const double nz = m.read<float>(normal, 8, start ? 0x420ac3 : 0x420a95);
    const double nx = m.read<float>(normal, 0, start ? 0x420ac8 : 0x420a9a);
    const double ny = m.read<float>(normal, 4, start ? 0x420acc : 0x420a9e);
    return (z * nz + x * nx) + y * ny; // 420A95..AA1: Z+X, thenY
}
bool band_420a70(Memory m, Query &q)
{
    const auto point = q.internals.plane_vertex.source(0x420a70);
    const auto normal = q.internals.plane_normal.source(0x420a75);
    q.internals.endpoint_plane_distance = f32(plane_projection(m, q.end, point, normal, false));
    q.internals.start_plane_distance = f32(plane_projection(m, q.start, point, normal, true));
    const double band = double(q.radius) + 0.25; // 46E24C=3E800000
    if (c03(std::abs(double(q.internals.endpoint_plane_distance)), band))
        return true;
    return c03(std::abs(double(q.internals.start_plane_distance)), band);
}
bool inside_420690(Memory m, Query &q, Ref polygon, const contact::Vector4 &candidate)
{
    auto descriptor = m.collision();
    const auto first = m.read<H>(polygon, 2, 0x4206a4);
    const auto indices = m.pointer(descriptor, 4, 0x4206a9);
    const auto first_index = m.read<H>(indices, std::uint64_t(first) * 2, 0x4206ae);
    const auto captured_indices = plus(indices, std::uint64_t(first) * 2);
    auto vertex = plus(m.pointer(descriptor, 8, 0x4206b8), std::uint64_t(first_index) * 16);
    const auto count = m.read<B>(polygon, 1, 0x4206c2);
    for (W ordinal = 0; ordinal < count;)
    {
        ++ordinal;
        const auto previous = vertex;
        const auto index = m.read<H>(captured_indices, std::uint64_t(ordinal % count) * 2, 0x4206ea);
        vertex = plus(m.pointer(descriptor, 8, 0x4206f3), std::uint64_t(index) * 16);
        const auto normal = q.internals.plane_normal.source(0x4206fb);
        const auto bx = m.read<float>(vertex, 0, 0x420703);
        const double dx = double(bx) - m.read<float>(previous, 0, 0x420705);
        const auto by = m.read<float>(vertex, 4, 0x420707);
        const double dy = double(by) - m.read<float>(previous, 4, 0x42070a);
        const auto bz = m.read<float>(vertex, 8, 0x42070d);
        const double dz = double(bz) - m.read<float>(previous, 8, 0x420710);
        const double yz = dy * m.read<float>(normal, 8, 0x420715);
        const double zy = dz * m.read<float>(normal, 4, 0x42071a);
        Vec3 edge;
        edge.x = f32(yz - zy);
        const double zx = dz * m.read<float>(normal, 0, 0x420722);
        const double xz = dx * m.read<float>(normal, 8, 0x420726);
        edge.y = f32(zx - xz);
        const double xy = dx * m.read<float>(normal, 4, 0x420730);
        const double yx = dy * m.read<float>(normal, 0, 0x420735);
        edge.z = f32(xy - yx);
        float length;
        normalize_44eda0(edge, length);
        const double cx = double(candidate.x) - m.read<float>(previous, 0, 0x420743);
        const double cy = double(candidate.y) - m.read<float>(previous, 4, 0x420748);
        const double cz = double(candidate.z) - m.read<float>(previous, 8, 0x42074e);
        const double distance = (cz * edge.z + cy * edge.y) + cx * edge.x;
        if (!c03(distance, 0.0))
            return false; // unrounded comparison after source FST
        if (ordinal < count)
            descriptor = m.collision(); // 4206D4, only loop back
    }
    return true; // count0 still reached first index/base reads, not vertex payload
}
bool accept_41ff30(Query &q, const contact::Vector4 &point)
{
    const double x = double(point.x) - q.start.x, y = double(point.y) - q.start.y, z = double(point.z) - q.start.z;
    const double distance = (double(q.direction.z) * z + double(q.direction.y) * y) + double(q.direction.x) * x;
    if (c0(distance, as_float(0xbc23d70a)))
        return false;
    if (!c0(distance, q.internals.candidate_distance))
        return false;
    q.internals.candidate_distance = f32(distance);
    return true;
}
bool face_4207a0(Memory m, Work &work, Query &q, Ref polygon)
{
    const double reach = double(q.radius) * 4.0;
    const auto mode = q.mode;
    q.center.x = 0;
    q.center.y = 0;
    q.center.z = 0;
    q.internals.candidate_distance = f32(reach * reach);
    // 4215B0 publishes mode2 and no child on its reached graph changes it.
    if (mode != 2)
        throw SourceAllocationError(0x4207f3, "420190/4205F0 are outside the reached4215B0 face-only graph");
    const auto normal = q.internals.plane_normal.source(0x420802);
    const double sx = double(q.step.x) * m.read<float>(normal, 0, 0x42080e);
    const double sy = double(q.step.y) * m.read<float>(normal, 4, 0x420816);
    const double sz = double(q.step.z) * m.read<float>(normal, 8, 0x420821);
    const float speed = f32((sx + sy) + sz);
    if (!c0(std::abs(double(q.internals.start_plane_distance) + speed), q.radius))
        return false;
    contact::Vector4 candidate;
    const bool copies_w = c0(std::abs(double(speed)), as_float(0x3a83126f));
    if (copies_w)
        candidate = q.start;
    else
    {
        const auto point = q.internals.plane_vertex.source(0x42087e);
        const double x = double(q.end.x) - m.read<float>(point, 0, 0x420889);
        const double y = double(q.end.y) - m.read<float>(point, 4, 0x420891);
        const double z = double(q.end.z) - m.read<float>(point, 8, 0x42089a);
        const double nx = m.read<float>(normal, 0, 0x42089f);
        const double ny = m.read<float>(normal, 4, 0x4208a3);
        const double nz = m.read<float>(normal, 8, 0x4208aa);
        const double distance = (x * nx + y * ny) + z * nz;
        double t = c0(q.internals.start_plane_distance, 0.0) ? (-distance - q.radius) / speed
                                                             : -((distance - q.radius) / speed);
        t -= as_float(0x38d1b717);
        if (c0(t, -1.0))
            t = -1.0;
        else if (!c03(t, 0.0))
            t = 0;
        const float tx = f32(double(q.step.x) * t), ty = f32(double(q.step.y) * t);
        const double tz = t * q.step.z;
        candidate.x = f32(double(q.end.x) + tx);
        candidate.y = f32(double(q.end.y) + ty);
        candidate.z = f32(double(q.end.z) + tz);
        // Original local W is undefined here. Defined XYZ only; persistent W is
        // retained, as in the already established contact/sampler contract.
        candidate.opaque = q.center.opaque;
    }
    if (!inside_420690(m, q, polygon, candidate) || !accept_41ff30(q, candidate))
        return false;
    const auto published_normal = q.internals.plane_normal.source(0x420974);
    const bool reverse = c0(q.internals.start_plane_distance, 0.0);
    const auto nx = m.read<float>(published_normal, 0, 0x420980);
    const auto ny = m.read<float>(published_normal, 4, 0x420988);
    work.last_normal.x = nx;
    work.last_normal.y = ny;
    work.last_normal.z = m.read<float>(published_normal, 8, 0x420996);
    work.last_normal.opaque = m.read<W>(published_normal, 12, 0x42099f);
    if (reverse)
    {
        work.last_normal.x = -work.last_normal.x;
        work.last_normal.y = -work.last_normal.y;
        work.last_normal.z = -work.last_normal.z;
    }
    const double radius = -double(q.radius);
    const double ox = radius * work.last_normal.x;
    q.center.z = candidate.z;
    q.center.x = candidate.x;
    q.surface.x = f32(ox);
    const double oy = radius * work.last_normal.y;
    q.center.y = candidate.y;
    q.center.opaque = candidate.opaque;
    q.polygon.bind_source(polygon); // 498164, after center W, before surfaceY spill
    q.surface.y = f32(oy);
    const double oz = radius * work.last_normal.z;
    q.surface.x = f32(double(q.surface.x) + candidate.x);
    q.surface.y = f32(double(q.surface.y) + candidate.y);
    q.surface.z = f32(oz + candidate.z);
    return true;
}
bool polygon_420c40(Memory m, Work &work, Query &q, Ref polygon)
{
    const auto normal_index = m.read<H>(polygon, 4, 0x420c46);
    const auto descriptor = m.collision();
    const auto normal = plus(m.pointer(descriptor, 12, 0x420c56), std::uint64_t(normal_index) * 16);
    const auto first = m.read<H>(polygon, 2, 0x420c5d);
    q.internals.plane_normal.bind_source(normal); // 420C61 before index dereference
    const auto indices = m.pointer(descriptor, 4, 0x420c67);
    const auto index = m.read<H>(indices, std::uint64_t(first) * 2, 0x420c6a);
    const auto vertex = plus(m.pointer(descriptor, 8, 0x420c6e), std::uint64_t(index) * 16);
    q.internals.plane_vertex.bind_source(vertex);
    if (!band_420a70(m, q))
        return false;
    return face_4207a0(m, work, q, polygon);
}
} // namespace
W source_camera_ground_query_4215b0(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(0x4215b0, "ground query requires actual source platform");
    auto &world = platform.state();
    auto &q = world.contact_cast;
    auto &work = world.contact_workspace;
    const auto y = q.start.y, x = q.start.x, z = q.start.z;
    q.end.y = y;
    const auto mx = work.query_motion.x;
    q.end.x = x;
    const auto w = q.start.opaque;
    q.end.z = z;
    const auto my = work.query_motion.y;
    q.direction.x = mx;
    const auto mw = q.motion_payload;
    q.end.opaque = w;
    const auto mz = work.query_motion.z;
    q.direction.y = my;
    q.direction.opaque = mw;
    q.mode = 2;
    q.hit_flag = 0;
    work.last_normal.x = 0;
    work.last_normal.y = 0;
    work.last_normal.z = 0;
    q.direction.z = mz;
    normalize_44eda0(q.direction, q.step_length);
    W steps = ceil_word(double(q.step_length) / (double(q.radius) + q.radius));
    const double inverse = 1.0 / double(steps); // source computes even for zero count
    const float ix = f32(inverse * work.query_motion.x), iy = f32(inverse * work.query_motion.y),
                iz = f32(inverse * work.query_motion.z);
    Memory memory{platform}; // no PMD/geometry access until a reached loop iteration
    while (steps != 0)
    {
        const double next_x = double(q.end.x) + ix;
        const auto ex = q.end.x, ey = q.end.y, ez = q.end.z;
        q.start.x = ex;
        q.start.y = ey;
        q.start.z = ez;
        q.end.x = f32(next_x);
        const double next_y = double(q.end.y) + iy;
        const auto ew = q.end.opaque;
        q.start.opaque = ew;
        q.end.y = f32(next_y);
        q.end.z = f32(double(q.end.z) + iz);
        q.step.x = f32(double(q.end.x) - q.start.x);
        q.step.y = f32(double(q.end.y) - q.start.y);
        const double dz = double(q.end.z) - q.start.z;
        q.step.z = f32(dz);
        q.step_length = f32(std::sqrt((dz * q.step.z + double(q.step.y) * q.step.y) + double(q.step.x) * q.step.x));
        select_420f80(memory, q);
        const auto descriptor = memory.collision();
        const auto list = q.polygon_list;
        const auto indices = memory.pointer(descriptor, 0, 0x421769);
        auto cursor = plus(indices, std::uint64_t(list) * 2);
        auto polygon_index = memory.read<H>(cursor, 0, 0x42176b);
        while (polygon_index != 0xffff)
        {
            const auto fresh = memory.collision();
            cursor = plus(cursor, 2);
            const auto polygons = memory.pointer(fresh, 16, 0x421786);
            const auto polygon = plus(polygons, std::uint64_t(polygon_index) * 8);
            if (polygon_420c40(memory, work, q, polygon))
            {
                const auto px = q.start.x, py = q.start.y, pz = q.start.z;
                q.end.x = px;
                const auto pw = q.start.opaque;
                q.end.opaque = pw;
                q.end.y = py;
                q.end.z = pz;
                q.mode = 1;
                return 1;
            }
            polygon_index = memory.read<H>(cursor, 0, 0x421796);
        }
        --steps;
    }
    const auto ex = q.end.x, ez = q.end.z, ey = q.end.y;
    q.start.x = ex;
    const auto ew = q.end.opaque;
    q.start.z = ez;
    q.start.y = ey;
    q.start.opaque = ew;
    q.mode = 1;
    return 0;
}
std::uint32_t SourceCameraGroundQuery::cast_source_track_particle_sphere_4215b0(RaceSessionPlatform &platform)
{
    return source_camera_ground_query_4215b0(platform);
}
} // namespace sgn::retail
