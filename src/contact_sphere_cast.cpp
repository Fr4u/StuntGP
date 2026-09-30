#include "sgn/contact_sphere_cast.hpp"
#include <algorithm>
#include <cmath>

namespace sgn::retail::contact
{
namespace
{
float f32(double x)
{
    return vehicle_f32(x);
}
Vec3 xyz(Vector4 v)
{
    return {v.x, v.y, v.z};
}
void set(Vector4 &v, Vec3 p)
{
    v.x = p.x;
    v.y = p.y;
    v.z = p.z;
}
bool finite(Vec3 p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
double dot_zyx(Vec3 a, Vec3 b)
{
    return (double(a.z) * b.z + double(a.y) * b.y) + double(a.x) * b.x;
}
Vec3 subtract(Vec3 a, Vec3 b)
{
    return {f32(double(a.x) - b.x), f32(double(a.y) - b.y), f32(double(a.z) - b.z)};
}
Vec3 normalize(Vec3 p, float *length = nullptr)
{
    const auto v = vehicle_normalize({p.x, p.y, p.z, 0}, length);
    return {v.x, v.y, v.z};
}
Vec3 source_cross(Vec3 a, Vec3 b)
{
    const auto v = vehicle_cross({a.x, a.y, a.z, 0}, {b.x, b.y, b.z, 0});
    return {v.x, v.y, v.z};
}
Vec3 offset(Vec3 base, Vec3 direction, double scale)
{
    return {f32(double(base.x) + f32(direction.x * scale)), f32(double(base.y) + f32(direction.y * scale)),
            f32(double(base.z) + direction.z * scale)};
}
double projected(Vec3 direction, Vec3 point, Vec3 start)
{
    return ((double(point.z) - start.z) * direction.z + (double(point.y) - start.y) * direction.y) +
           (double(point.x) - start.x) * direction.x;
}

// Each accepted vertex/edge/face writes the persistent source globals here.
// The outer candidate selector separately saves/restores its closest result.
struct Kernel
{
    const TrackGeometry &track;
    solver::Workspace &work;
    ContactCastState &s;
    ContactCastPayloadAudit *audit;
    ContactQueryInternalsAudit *internals_audit;
    solver::PayloadAudit *payload;
    bool accept(Vec3 point)
    { // 41FF30
        const double distance = projected(xyz(s.direction), point, xyz(s.start));
        if (!(distance >= double(-0.01f) && distance < s.internals.candidate_distance))
            return false;
        s.internals.candidate_distance = f32(distance);
        return true;
    }
    void computed_contact(Vec3 normal, Vec3 center, Vec3 surface)
    {
        set(work.last_normal, normal);
        set(s.center, center);
        set(s.surface, surface);
        if (payload)
            payload->normal = true;
        if (audit)
            audit->center = true;
    }
    bool vertex(Vec3 point)
    { // 41FFA0
        const double x = double(point.x) - s.start.x, y = double(point.y) - s.start.y, z = double(point.z) - s.start.z;
        const double distance = (z * z + y * y) + x * x;
        const float stored = f32(distance);
        const double reach = double(s.step_length) + s.radius;
        if (distance > reach * reach)
            return false;
        const double along = (z * s.direction.z + y * s.direction.y) + x * s.direction.x;
        const double perpendicular = double(stored) - along * along;
        if (perpendicular > s.radius_squared)
            return false;
        const double t = along - std::sqrt(double(s.radius_squared) - perpendicular);
        const auto center = offset(xyz(s.start), xyz(s.direction), t);
        if (!accept(center))
            return false;
        const auto normal = normalize(subtract(center, point));
        computed_contact(normal, center,
                         {f32(s.radius * double(normal.x) + point.x),
                          f32(double(f32(s.radius * double(normal.y))) + point.y),
                          f32(double(f32(s.radius * double(normal.z))) + point.z)});
        s.internals.auxiliary = s.surface;
        if (internals_audit)
            internals_audit->auxiliary = false;
        return true;
    }
    bool edge(Vec3 a, Vec3 b)
    { // 420200
        float length{};
        const auto axis = normalize(subtract(b, a), &length), delta = subtract(xyz(s.start), a);
        const float along = f32(dot_zyx(delta, axis));
        const double movement = dot_zyx(xyz(s.step), axis);
        if (-movement > along || double(along) > movement + length)
            return false;
        const double px = double(delta.x) - double(along) * axis.x, py = double(delta.y) - f32(double(along) * axis.y),
                     pz = double(delta.z) - f32(double(along) * axis.z);
        if (std::sqrt((pz * pz + py * py) + px * px) > double(s.radius) + s.step_length)
            return false;
        const auto direction = s.step_length < float(0.00001f) ? delta : xyz(s.step);
        const auto perpendicular = normalize(source_cross(direction, axis), &length);
        const double distance = std::abs(dot_zyx(perpendicular, delta));
        const float stored_distance = f32(distance);
        if (!(distance < s.radius))
            return false;
        const double cx = double(delta.y) * axis.z - double(delta.z) * axis.y,
                     cy = double(delta.z) * axis.x - double(delta.x) * axis.z,
                     cz = double(delta.x) * axis.y - double(delta.y) * axis.x;
        const float shift = f32(-((cz * perpendicular.z + cy * perpendicular.y) + cx * perpendicular.x) / length);
        const auto perpendicular2 = normalize(source_cross(perpendicular, axis));
        const double denominator = (double(perpendicular2.x) * direction.x + double(perpendicular2.z) * direction.z) +
                                   double(perpendicular2.y) * direction.y;
        double t =
            std::abs(std::sqrt(double(s.radius_squared) - double(stored_distance) * stored_distance) / denominator);
        if (t < double(0.0001f))
            t = 0;
        t = double(shift) - t;
        const auto center = offset(xyz(s.start), direction, t);
        if (!accept(center))
            return false;
        const double dx = double(center.x) - a.x, dz = double(center.z) - a.z;
        const float dy = f32(double(center.y) - a.y), stored_z = f32(dz);
        const double projection = (dz * axis.z + double(dy) * axis.y) + dx * axis.x;
        const Vec3 along_axis{f32(projection * axis.x), f32(projection * axis.y), f32(projection * axis.z)};
        const auto normal =
            normalize({f32(dx - along_axis.x), f32(double(dy) - along_axis.y), f32(double(stored_z) - along_axis.z)});
        computed_contact(
            normal, center,
            {f32(double(along_axis.x) + a.x), f32(double(along_axis.y) + a.y), f32(double(along_axis.z) + a.z)});
        set(s.internals.auxiliary, center);
        if (internals_audit)
            internals_audit->auxiliary = true;
        return true;
    }
    bool polygon(std::uint32_t index)
    { // 420C40 +4207A0
        const auto &p = track.polygons.at(index);
        const auto vertex_index = track.indices.at(p.first_index);
        s.internals.plane_vertex = vertex_index;
        s.internals.plane_normal = p.normal;
        const auto point = track.vertices.at(vertex_index), normal = track.normals.at(p.normal);
        const auto band = plane_band(xyz(s.start), xyz(s.end), point, normal, s.radius);
        s.internals.endpoint_plane_distance = band.endpoint_distance;
        s.internals.start_plane_distance = band.start_distance;
        if (!band.accepted)
            return false;
        s.internals.candidate_distance = f32((double(s.radius) * 4) * (double(s.radius) * 4));
        set(s.center, {});
        bool found = false;
        if (s.mode != 2)
        {
            for (unsigned j = 0; j < p.vertex_count; ++j)
                if (vertex(track.vertices.at(track.indices.at(p.first_index + j))))
                {
                    found = true;
                    s.polygon = index;
                    s.internals.contact_kind = 3;
                }
            for (unsigned j = 0; j < p.vertex_count; ++j)
                if (edge(track.vertices.at(track.indices.at(p.first_index + j)),
                         track.vertices.at(track.indices.at(p.first_index + (j + 1) % p.vertex_count))))
                {
                    found = true;
                    s.polygon = index;
                    s.internals.contact_kind = 2;
                }
        }
        const float speed =
            f32((double(s.step.x) * normal.x + double(s.step.y) * normal.y) + double(s.step.z) * normal.z);
        if (!(std::abs(double(band.start_distance) + speed) < s.radius))
            return found;
        auto center = xyz(s.start);
        const bool inherits = std::abs(double(speed)) < double(0.001f);
        if (!inherits)
        {
            const double distance = ((double(s.end.x) - point.x) * normal.x + (double(s.end.y) - point.y) * normal.y) +
                                    (double(s.end.z) - point.z) * normal.z;
            double t = band.start_distance < 0 ? (-distance - s.radius) / speed : -((distance - s.radius) / speed);
            t = std::clamp(t - double(0.0001f), -1.0, 0.0);
            center = offset(xyz(s.end), xyz(s.step), t);
        }
        if (!inside_polygon(track, p, center) || !accept(center))
            return found;
        auto n = normal;
        if (band.start_distance < 0)
            n = {-n.x, -n.y, -n.z};
        set(work.last_normal, n);
        work.last_normal.opaque = track.normal_payload.at(p.normal);
        if (payload)
            payload->normal = false;
        set(s.center, center);
        if (inherits)
            s.center.opaque = s.start.opaque;
        if (audit)
            audit->center = inherits ? audit->start : true;
        set(s.surface, {f32(double(f32(-double(s.radius) * n.x)) + center.x),
                        f32(double(f32(-double(s.radius) * n.y)) + center.y), f32(-double(s.radius) * n.z + center.z)});
        s.polygon = index;
        return true;
    }
    bool candidates()
    { // 420CA0
        float closest = s.step_length;
        s.polygon.reset();
        bool found = false;
        Vector4 chosen_center{}, chosen_normal{}, chosen_surface{}, chosen_auxiliary{};
        std::optional<std::uint32_t> chosen_polygon;
        bool center_undefined{}, normal_undefined{}, auxiliary_undefined{};
        for (std::size_t at = s.polygon_list;; ++at)
        {
            const auto index = track.polygon_lists.at(at);
            if (index == 0xffff)
                break;
            set(work.last_normal, {});
            if (!polygon(index))
                continue;
            const double distance = projected(xyz(s.direction), xyz(s.center), xyz(s.start));
            if (distance < double(-0.01f) || !(distance < closest))
                continue;
            closest = f32(distance);
            chosen_center = s.center;
            chosen_normal = work.last_normal;
            chosen_surface = s.surface;
            chosen_auxiliary = s.internals.auxiliary;
            chosen_polygon = s.polygon;
            found = true;
            if (audit)
                center_undefined = audit->center;
            if (payload)
                normal_undefined = payload->normal;
            if (internals_audit)
                auxiliary_undefined = internals_audit->auxiliary;
        }
        if (found)
        {
            s.center = chosen_center;
            work.last_normal = chosen_normal;
            s.surface = chosen_surface;
            s.internals.auxiliary = chosen_auxiliary;
            s.polygon = chosen_polygon;
            s.hit_flag = 1;
            if (audit)
                audit->center = center_undefined;
            if (payload)
                payload->normal = normal_undefined;
            if (internals_audit)
                internals_audit->auxiliary = auxiliary_undefined;
        }
        return found;
    }
};
} // namespace
namespace query_detail
{
void select_polygon_list(const TrackGeometry &track, ContactCastState &s)
{
    s.internals.tree_tier = s.radius > track.radius_threshold[0] ? 1u : 0u;
    s.polygon_list = find_polygon_list(track, xyz(s.end), s.radius);
}
bool test_polygon(const TrackGeometry &track, solver::Workspace &work, ContactCastState &s, std::uint32_t index,
                  ContactCastPayloadAudit *audit, ContactQueryInternalsAudit *internals_audit,
                  solver::PayloadAudit *payload)
{
    return Kernel{track, work, s, audit, internals_audit, payload}.polygon(index);
}
bool choose_closest(const TrackGeometry &track, solver::Workspace &work, ContactCastState &s,
                    ContactCastPayloadAudit *audit, ContactQueryInternalsAudit *internals_audit,
                    solver::PayloadAudit *payload)
{
    return Kernel{track, work, s, audit, internals_audit, payload}.candidates();
}
} // namespace query_detail
std::uint32_t cast_track_sphere(const TrackGeometry &track, solver::Workspace &work, ContactCastState &s,
                                ContactCastPayloadAudit *audit, ContactQueryInternalsAudit *internals_audit,
                                solver::PayloadAudit *payload)
{
    if (!finite(xyz(s.start)) || !finite(work.query_motion) || !std::isfinite(s.radius) || s.radius <= 0 ||
        !std::isfinite(s.radius_squared) || !std::isfinite(s.remaining))
        throw FormatError("invalid persistent sphere query");
    const auto initial = s.start;
    const Vec3 displacement{f32(double(s.remaining) * work.query_motion.x),
                            f32(double(s.remaining) * work.query_motion.y),
                            f32(double(s.remaining) * work.query_motion.z)};
    s.end = s.start;
    s.hit_flag = 0;
    float total_length{};
    set(s.direction, normalize(displacement, &total_length));
    if (audit)
    {
        audit->end = audit->start;
        audit->direction = true;
    }
    const float steps = f32(std::ceil(double(total_length) / (double(s.radius) + s.radius)));
    if (!std::isfinite(steps) || steps > 65536)
        throw FormatError("persistent sphere query exceeds bounded workload");
    Kernel kernel{track, work, s, audit, internals_audit, payload};
    bool found = false;
    for (unsigned step = 1; step <= static_cast<unsigned>(steps); ++step)
    {
        s.start = s.end;
        if (audit)
            audit->start = audit->end;
        set(s.end, offset(xyz(initial), displacement, double(step) / steps));
        s.step.x = f32(double(s.end.x) - s.start.x);
        s.step.y = f32(double(s.end.y) - s.start.y);
        const double dz = double(s.end.z) - s.start.z;
        s.step.z = f32(dz);
        s.step_length = f32(std::sqrt((dz * s.step.z + double(s.step.y) * s.step.y) + double(s.step.x) * s.step.x));
        query_detail::select_polygon_list(track, s);
        found = kernel.candidates();
        if (found)
            break;
    }
    if (!found)
        return 0;
    const double dx = double(s.center.x) - initial.x, dy = double(s.center.y) - initial.y,
                 dz = double(s.center.z) - initial.z;
    const double distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
    s.remaining = f32((double(total_length) - distance) / total_length);
    s.start = s.center;
    if (audit)
        audit->start = audit->center;
    s.start.x = f32(double(s.center.x) + double(0.1f) * work.last_normal.x);
    s.start.y = f32(double(s.center.y) + f32(double(0.1f) * work.last_normal.y));
    s.start.z = f32(double(s.center.z) + f32(double(0.1f) * work.last_normal.z));
    return 1;
}
} // namespace sgn::retail::contact
