#include "sgn/track_collision.hpp"
#include <algorithm>
#include <cmath>

namespace sgn::retail::contact
{
namespace
{
float f32(double x)
{
    return static_cast<float>(x);
}
double dot_zyx(Vec3 a, Vec3 b)
{
    return (double(a.z) * b.z + double(a.y) * b.y) + double(a.x) * b.x;
}
Vec3 sub(Vec3 a, Vec3 b)
{
    return {f32(double(a.x) - b.x), f32(double(a.y) - b.y), f32(double(a.z) - b.z)};
}
Vec3 cross32(Vec3 a, Vec3 b)
{
    return {f32(double(a.y) * b.z - double(a.z) * b.y), f32(double(a.z) * b.x - double(a.x) * b.z),
            f32(double(a.x) * b.y - double(a.y) * b.x)};
}
Vec3 normalize(Vec3 v, float *stored_length = nullptr)
{
    const double squared = (double(v.x) * v.x + double(v.y) * v.y) + double(v.z) * v.z;
    if (squared == 0)
    {
        if (stored_length)
            *stored_length = 0;
        return {};
    }
    const double length = std::sqrt(squared);
    if (stored_length)
        *stored_length = f32(length);
    const double scale = 1 / length;
    return {f32(v.x * scale), f32(v.y * scale), f32(v.z * scale)};
}
Vec3 offset_xy_stored(Vec3 base, Vec3 v, double scale)
{
    return {f32(double(base.x) + f32(v.x * scale)), f32(double(base.y) + f32(v.y * scale)),
            f32(double(base.z) + v.z * scale)};
}
double projected_distance(Vec3 direction, Vec3 point, Vec3 start)
{
    return ((double(point.z) - start.z) * direction.z + (double(point.y) - start.y) * direction.y) +
           (double(point.x) - start.x) * direction.x;
}
struct Candidate
{
    Vec3 normal{}, center{}, surface{};
    std::uint32_t normal_payload{};
    bool normal_payload_defined{};
    bool center_payload_inherits_input{};
};
struct Kernel
{
    const TrackGeometry &track;
    SweepResult &state;
    float radius, radius_squared, best{};
    Candidate candidate;
    bool include_edges{true};
    bool accept(Vec3 point)
    { // 0041FF30
        const double distance = projected_distance(state.direction, point, state.start);
        // Unordered x87 comparisons reject parallel-edge NaNs. Negating two
        // separate inequalities would incorrectly accept NaN and erase a hit.
        if (!(distance >= double(-0.01f) && distance < best))
            return false;
        best = f32(distance);
        return true;
    }
    bool vertex(Vec3 point)
    { // 0041FFA0
        const double x = double(point.x) - state.start.x, y = double(point.y) - state.start.y,
                     z = double(point.z) - state.start.z;
        const double distance = (z * z + y * y) + x * x;
        const float stored = f32(distance);
        const double reach = double(state.step_length) + radius;
        if (distance > reach * reach)
            return false;
        const double along = (z * state.direction.z + y * state.direction.y) + x * state.direction.x;
        const double perpendicular = double(stored) - along * along;
        if (perpendicular > radius_squared)
            return false;
        const double t = along - std::sqrt(double(radius_squared) - perpendicular);
        auto center = offset_xy_stored(state.start, state.direction, t);
        if (!accept(center))
            return false;
        auto normal = normalize(sub(center, point));
        candidate = {normal,
                     center,
                     {f32(radius * double(normal.x) + point.x), f32(double(f32(radius * double(normal.y))) + point.y),
                      f32(double(f32(radius * double(normal.z))) + point.z)}};
        return true;
    }
    bool edge(Vec3 a, Vec3 b)
    { // 00420200; branch order follows the original.
        float length{};
        auto axis = normalize(sub(b, a), &length);
        const auto delta = sub(state.start, a);
        const float along = f32(dot_zyx(delta, axis));
        const double movement = dot_zyx(state.step, axis);
        if (-movement > along || double(along) > movement + length)
            return false;
        const double px = double(delta.x) - double(along) * axis.x;
        const double py = double(delta.y) - f32(double(along) * axis.y),
                     pz = double(delta.z) - f32(double(along) * axis.z);
        if (std::sqrt((pz * pz + py * py) + px * px) > double(radius) + state.step_length)
            return false;
        const auto direction = state.step_length < float(0.00001f) ? delta : state.step;
        auto perpendicular = normalize(cross32(direction, axis), &length);
        const double dist = std::abs(dot_zyx(perpendicular, delta));
        const float stored_dist = f32(dist);
        if (!(dist < radius))
            return false;
        // The original reuses 'length' for the cross product length at 004203A7.
        const double cx = double(delta.y) * axis.z - double(delta.z) * axis.y;
        const double cy = double(delta.z) * axis.x - double(delta.x) * axis.z;
        const double cz = double(delta.x) * axis.y - double(delta.y) * axis.x;
        const float offset = f32(-((cz * perpendicular.z + cy * perpendicular.y) + cx * perpendicular.x) / length);
        auto perpendicular2 = normalize(cross32(perpendicular, axis));
        const double denom = (double(perpendicular2.x) * direction.x + double(perpendicular2.z) * direction.z) +
                             double(perpendicular2.y) * direction.y;
        double t = std::abs(std::sqrt(double(radius_squared) - double(stored_dist) * stored_dist) / denom);
        if (t < double(0.0001f))
            t = 0;
        t = double(offset) - t;
        auto center = offset_xy_stored(state.start, direction, t);
        if (!accept(center))
            return false;
        const double dx = double(center.x) - a.x, dz = double(center.z) - a.z;
        const float dy = f32(double(center.y) - a.y), sz = f32(dz);
        const double projection = (dz * axis.z + double(dy) * axis.y) + dx * axis.x;
        Vec3 projected{f32(projection * axis.x), f32(projection * axis.y), f32(projection * axis.z)};
        auto n = normalize({f32(dx - projected.x), f32(double(dy) - projected.y), f32(double(sz) - projected.z)});
        candidate = {n,
                     center,
                     {f32(double(projected.x) + a.x), f32(double(projected.y) + a.y), f32(double(projected.z) + a.z)}};
        return true;
    }
    bool polygon(const Polygon &p)
    { // 00420C40/004207A0
        const auto point = track.vertices.at(track.indices.at(p.first_index)), normal = track.normals.at(p.normal);
        const auto band = plane_band(state.start, state.end, point, normal, radius);
        if (!band.accepted)
            return false;
        best = f32((double(radius) * 4) * (double(radius) * 4));
        candidate.center = {};
        bool found = false;
        if (include_edges)
        {
            for (unsigned i = 0; i < p.vertex_count; ++i)
                found = vertex(track.vertices.at(track.indices.at(p.first_index + i))) || found;
            for (unsigned i = 0; i < p.vertex_count; ++i)
                found = edge(track.vertices.at(track.indices.at(p.first_index + i)),
                             track.vertices.at(track.indices.at(p.first_index + (i + 1) % p.vertex_count))) ||
                        found;
        }
        const float speed =
            f32((double(state.step.x) * normal.x + double(state.step.y) * normal.y) + double(state.step.z) * normal.z);
        if (!(std::abs(double(band.start_distance) + speed) < radius))
            return found;
        Vec3 center = state.start;
        if (std::abs(double(speed)) >= double(0.001f))
        {
            const double distance =
                ((double(state.end.x) - point.x) * normal.x + (double(state.end.y) - point.y) * normal.y) +
                (double(state.end.z) - point.z) * normal.z;
            double t = band.start_distance < 0 ? (-distance - radius) / speed : -((distance - radius) / speed);
            t = std::clamp(t - double(0.0001f), -1.0, 0.0);
            center = offset_xy_stored(state.end, state.step, t);
        }
        if (!inside_polygon(track, p, center) || !accept(center))
            return found;
        auto n = normal;
        if (band.start_distance < 0)
            n = {-n.x, -n.y, -n.z};
        candidate = {n,
                     center,
                     {f32(double(f32(-double(radius) * n.x)) + center.x),
                      f32(double(f32(-double(radius) * n.y)) + center.y), f32(-double(radius) * n.z + center.z)}};
        // 0042099F..004209A2 copies the authored fourth normal word. Edge and
        // vertex normals instead carry an uninitialized temporary fourth word.
        if (p.normal < track.normal_payload.size())
        {
            candidate.normal_payload = track.normal_payload[p.normal];
            candidate.normal_payload_defined = true;
        }
        candidate.center_payload_inherits_input = std::abs(double(speed)) < double(0.001f);
        return true;
    }
    bool candidates(std::uint32_t start)
    { // 00420CA0
        float closest = state.step_length;
        bool found = false;
        for (std::size_t at = start;; ++at)
        {
            auto id = track.polygon_lists.at(at);
            if (id == 0xffff)
                break;
            const auto &p = track.polygons.at(id);
            state.query_normal_written = true;
            state.query_normal = {};
            if (!polygon(p))
                continue;
            state.query_normal = candidate.normal;
            state.query_normal_payload_written = true;
            state.query_normal_payload = candidate.normal_payload;
            state.query_normal_payload_defined = candidate.normal_payload_defined;
            const double distance = projected_distance(state.direction, candidate.center, state.start);
            if (distance < double(-0.01f) || !(distance < closest))
                continue;
            closest = f32(distance);
            state.normal = candidate.normal;
            state.center = candidate.center;
            state.surface = candidate.surface;
            state.polygon = id;
            state.normal_payload = candidate.normal_payload;
            state.normal_payload_defined = candidate.normal_payload_defined;
            found = true;
            state.start_payload_inherits_input = candidate.center_payload_inherits_input;
        }
        if (found)
        {
            state.query_normal = state.normal;
            state.query_normal_payload_written = true;
            state.query_normal_payload = state.normal_payload;
            state.query_normal_payload_defined = state.normal_payload_defined;
        }
        return found;
    }
};
} // namespace
PolygonHit intersect_polygon(const TrackGeometry &track, std::uint32_t polygon, const PolygonQuery &input)
{
    SweepResult state;
    state.start = input.start;
    state.end = input.end;
    state.step = input.step;
    state.direction = input.direction;
    state.step_length = input.step_length;
    Kernel kernel{track, state, input.radius, input.radius_squared, 0, {}, input.include_edges};
    const bool hit = kernel.polygon(track.polygons.at(polygon));
    const auto &c = kernel.candidate;
    return {hit, c.normal, c.center, c.surface, kernel.best, c.normal_payload, c.normal_payload_defined};
}
PolygonHit intersect_edge(Vec3 a, Vec3 b, const PolygonQuery &input, float previous_distance)
{
    const TrackGeometry unused;
    SweepResult state;
    state.start = input.start;
    state.end = input.end;
    state.step = input.step;
    state.direction = input.direction;
    state.step_length = input.step_length;
    Kernel kernel{unused, state, input.radius, input.radius_squared, previous_distance, {}};
    const bool hit = kernel.edge(a, b);
    const auto &c = kernel.candidate;
    return {hit, c.normal, c.center, c.surface, kernel.best, c.normal_payload, c.normal_payload_defined};
}
SweepResult sweep_sphere(const TrackGeometry &track, const SweepInput &input)
{
    auto finite = [](Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
    // 00420FE0 multiplies by the incoming scale without clamping. A hit up to
    // 0.01 behind the start can produce a negative remainder, which the caller
    // passes into its next query. Values above one are also accepted by retail.
    if (!finite(input.start) || !finite(input.motion) || !std::isfinite(input.radius) || input.radius <= 0 ||
        !std::isfinite(input.remaining))
        throw FormatError("invalid track sphere sweep");
    SweepResult state;
    state.start = input.start;
    state.end = input.start;
    state.remaining = input.remaining;
    Vec3 displacement{f32(double(input.remaining) * input.motion.x), f32(double(input.remaining) * input.motion.y),
                      f32(double(input.remaining) * input.motion.z)};
    state.direction = normalize(displacement, &state.travel_length);
    const float steps = f32(std::ceil(double(state.travel_length) / (double(input.radius) + input.radius)));
    if (steps > 65536)
        throw FormatError("track sweep exceeds bounded query workload");
    Kernel kernel{track, state, input.radius, f32(double(input.radius) * input.radius), 0, {}};
    for (unsigned step = 1; step <= static_cast<unsigned>(steps); ++step)
    {
        state.start = state.end;
        const double fraction = double(step) / steps;
        state.end = offset_xy_stored(input.start, displacement, fraction);
        state.step = sub(state.end, state.start);
        state.step_length = f32(std::sqrt(dot_zyx(state.step, state.step)));
        state.hit = kernel.candidates(find_polygon_list(track, state.end, input.radius));
        if (state.hit)
            break;
    }
    if (state.hit)
    {
        const double dx = double(state.center.x) - input.start.x, dy = double(state.center.y) - input.start.y,
                     dz = double(state.center.z) - input.start.z;
        const double traveled = std::sqrt((dz * dz + dy * dy) + dx * dx);
        state.remaining = f32((double(state.travel_length) - traveled) / state.travel_length);
        state.start = {f32(double(state.center.x) + double(0.1f) * state.normal.x),
                       f32(double(state.center.y) + f32(double(0.1f) * state.normal.y)),
                       f32(double(state.center.z) + f32(double(0.1f) * state.normal.z))};
    }
    return state;
}
} // namespace sgn::retail::contact
