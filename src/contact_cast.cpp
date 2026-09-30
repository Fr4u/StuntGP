#include "sgn/contact_sphere_cast.hpp"
#include <cmath>
namespace sgn::retail::contact
{
namespace
{
float f32(double x)
{
    return vehicle_f32(x);
}

} // namespace
std::uint32_t cast_track_faces(const TrackGeometry &track, solver::Workspace &work, ContactCastState &s,
                               ContactCastPayloadAudit *audit, ContactQueryInternalsAudit *internals_audit,
                               solver::PayloadAudit *payload)
{
    s.mode = 2;
    if (!(s.remaining >= 0 && s.remaining <= 1))
        return 0;
    if (!(s.radius > 0) || !std::isfinite(s.radius))
        throw FormatError("face cast radius outside finite positive domain");
    const auto initial = s.start;
    const Vec3 motion{f32(double(work.query_motion.x) * s.remaining), f32(double(work.query_motion.y) * s.remaining),
                      f32(double(work.query_motion.z) * s.remaining)};
    s.end = s.start;
    s.hit_flag = 0;
    if (audit)
    {
        audit->end = audit->start;
        audit->direction = true;
    }
    float length{};
    const auto direction = vehicle_normalize({motion.x, motion.y, motion.z, 0}, &length);
    s.direction.x = direction.x;
    s.direction.y = direction.y;
    s.direction.z = direction.z;
    const float steps = f32(std::ceil(double(length) / (double(s.radius) + s.radius)));
    if (!std::isfinite(steps) || steps > 65536)
        throw FormatError("face cast exceeds bounded workload");
    bool found = false;
    for (unsigned step = 1; step <= static_cast<unsigned>(steps); ++step)
    {
        s.start = s.end;
        if (audit)
            audit->start = audit->end;
        const double fraction = double(step) / steps;
        s.end.x = f32(double(initial.x) + f32(fraction * motion.x));
        s.end.y = f32(double(initial.y) + f32(fraction * motion.y));
        s.end.z = f32(double(initial.z) + fraction * motion.z);
        s.step.x = f32(double(s.end.x) - s.start.x);
        s.step.y = f32(double(s.end.y) - s.start.y);
        const double dz = double(s.end.z) - s.start.z;
        s.step.z = f32(dz);
        s.step_length = f32(std::sqrt((dz * s.step.z + double(s.step.y) * s.step.y) + double(s.step.x) * s.step.x));
        query_detail::select_polygon_list(track, s);
        found = query_detail::choose_closest(track, work, s, audit, internals_audit, payload);
        if (found)
            break;
    }
    s.mode = 1;
    if (!found)
        return 0;
    const double dx = double(s.center.x) - initial.x, dy = double(s.center.y) - initial.y,
                 dz = double(s.center.z) - initial.z;
    const double distance = std::sqrt((dz * dz + dy * dy) + dx * dx);
    s.remaining = f32((double(length) - distance) / length);
    s.start = s.center;
    if (audit)
        audit->start = audit->center;
    s.start.x = f32(double(s.center.x) + double(0.1f) * work.last_normal.x);
    s.start.y = f32(double(s.center.y) + f32(double(0.1f) * work.last_normal.y));
    s.start.z = f32(double(s.center.z) + f32(double(0.1f) * work.last_normal.z));
    return 1;
}
} // namespace sgn::retail::contact
