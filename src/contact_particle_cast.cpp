#include "sgn/impact_particle_lifetime.hpp"
#include "sgn/contact_sphere_cast.hpp"
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
float f32(double value)
{
    return vehicle_f32(value);
}
Vec3 xyz(Vector4 value)
{
    return {value.x, value.y, value.z};
}
bool finite(Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace
std::uint32_t cast_track_particle_sphere(const TrackGeometry &track, solver::Workspace &work, ContactCastState &s,
                                         ContactCastPayloadAudit *audit, solver::PayloadAudit *payload,
                                         ContactQueryInternalsAudit *internals_audit)
{
    if (!finite(xyz(s.start)) || !finite(work.query_motion) || !std::isfinite(s.radius) || s.radius <= 0)
        throw FormatError("invalid particle sphere query");
    s.end = s.start;
    s.direction = {work.query_motion.x, work.query_motion.y, work.query_motion.z, s.motion_payload};
    s.mode = 2;
    s.hit_flag = 0;
    work.last_normal.x = 0;
    work.last_normal.y = 0;
    work.last_normal.z = 0;
    if (audit)
    {
        audit->end = audit->start;
        audit->direction = audit->motion;
    }
    const auto normalized = vehicle_normalize({s.direction.x, s.direction.y, s.direction.z, 0}, &s.step_length);
    s.direction.x = normalized.x;
    s.direction.y = normalized.y;
    s.direction.z = normalized.z;
    const double steps = std::ceil(double(s.step_length) / (double(s.radius) + s.radius));
    if (!std::isfinite(steps) || steps > 65536)
        throw FormatError("particle sphere query exceeds bounded workload");
    if (steps == 0)
    {
        s.start = s.end;
        if (audit)
            audit->start = audit->end;
        s.mode = 1;
        return 0;
    }
    const double fraction = 1 / steps;
    const Vec3 increment{f32(fraction * work.query_motion.x), f32(fraction * work.query_motion.y),
                         f32(fraction * work.query_motion.z)};
    for (unsigned step = 0; step < static_cast<unsigned>(steps); ++step)
    {
        s.start = s.end;
        if (audit)
            audit->start = audit->end;
        s.end.x = f32(double(s.end.x) + increment.x);
        s.end.y = f32(double(s.end.y) + increment.y);
        s.end.z = f32(double(s.end.z) + increment.z);
        s.step.x = f32(double(s.end.x) - s.start.x);
        s.step.y = f32(double(s.end.y) - s.start.y);
        const double dz = double(s.end.z) - s.start.z;
        s.step.z = f32(dz);
        s.step_length = f32(std::sqrt((dz * s.step.z + double(s.step.y) * s.step.y) + double(s.step.x) * s.step.x));
        query_detail::select_polygon_list(track, s);
        for (std::size_t at = s.polygon_list;; ++at)
        {
            const auto index = track.polygon_lists.at(at);
            if (index == 0xffff)
                break;
            if (!query_detail::test_polygon(track, work, s, index, audit, internals_audit, payload))
                continue;
            s.end = s.start;
            if (audit)
                audit->end = audit->start;
            s.mode = 1;
            return 1;
        }
    }
    s.start = s.end;
    if (audit)
        audit->start = audit->end;
    s.mode = 1;
    return 0;
}
} // namespace sgn::retail::contact::effects
