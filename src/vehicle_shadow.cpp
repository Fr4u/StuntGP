#include "sgn/vehicle_shadow.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return vehicle_f32(value);
}
void normalize(contact::Vector4 &value)
{
    const auto result = vehicle_normalize({value.x, value.y, value.z, 0});
    value.x = result.x;
    value.y = result.y;
    value.z = result.z;
}
} // namespace
void update_vehicle_shadow(const VehicleState &s, VehiclePresentationState &p, VehicleShadowState &shadow,
                           bool available, const VehicleShadowLighting &lighting, std::uint32_t tick,
                           contact::player::VehicleId id, const contact::player::Controls &controls, Vec3 gravity,
                           const contact::TrackGeometry &track, contact::solver::Workspace &work,
                           contact::ContactCastState &query, contact::ContactCastPayloadAudit *audit)
{
    if (!available || lighting.active_light_count == 0)
    {
        p.field_3924 = 0;
        return;
    }
    if (tick < 60)
    {
        bool controlled = false;
        for (const auto &controller : controls.controllers)
            if (controller.active == 1 && controller.vehicle == id)
                controlled = true;
        if (!controlled)
        {
            p.field_3924 = 0;
            return;
        }
    }
    if (tick < 50)
    {
        p.field_3924 = 0;
        return;
    }
    p.field_3924 = 1;
    const auto &w = p.wheel_centers;
    std::array<contact::Vector4, 3> points{
        {{w[0].x, w[0].y, w[0].z, std::bit_cast<std::uint32_t>(w[0].w)},
         {w[1].x, w[1].y, w[1].z, std::bit_cast<std::uint32_t>(w[1].w)},
         {f32((double(w[3].x) + w[2].x) * .5), f32(double(f32(double(w[3].y) + w[2].y)) * .5),
          f32(double(f32(double(w[3].z) + w[2].z)) * .5), query.start.opaque}}};
    auto light = lighting.position;
    if (light.y < 75000)
        light.y = 75000;
    contact::Vector4 sum{};
    for (unsigned i = 0; i < 3; ++i)
    {
        auto &point = points[i];
        auto direction = vehicle_normalize(
            {f32(double(point.x) - light.x), f32(double(point.y) - light.y), f32(double(point.z) - light.z), 0});
        work.query_motion = {f32(double(direction.x) * 2000), f32(double(direction.y) * 2000),
                             f32(double(direction.z) * 2000)};
        query.radius = 50;
        query.radius_squared = 2500;
        query.travel_limit = 2000;
        query.remaining = 1;
        const auto old_start_w = query.start.opaque;
        query.start = point;
        if (i == 2)
            query.start.opaque = old_start_w;
        if (audit)
        {
            audit->start = i == 2;
            audit->motion = true;
        }
        if (contact::cast_track_faces(track, work, query, audit) != 1)
        {
            p.field_3924 = 0;
            return;
        }
        point = query.center;
        sum.x = f32(double(sum.x) + point.x);
        sum.y = f32(double(sum.y) + point.y);
        sum.z = f32(double(sum.z) + point.z);
    }
    const float third = std::bit_cast<float>(std::uint32_t{0x3eaaaaab});
    shadow.position.x = f32(double(sum.x) * third);
    shadow.position.y = f32(double(sum.y) * third);
    shadow.position.z = f32(double(sum.z) * third);
    if (audit)
        audit->shadow_position = true;
    const float mx = f32((double(points[1].x) + points[0].x) * .5);
    const float my = f32(double(f32(double(points[1].y) + points[0].y)) * .5),
                mz = f32(double(f32(double(points[1].z) + points[0].z)) * .5);
    shadow.forward.x = f32(double(points[2].x) - mx);
    shadow.forward.y = f32(double(points[2].y) - my);
    shadow.forward.z = f32(double(points[2].z) - mz);
    normalize(shadow.forward);
    const Vec3 a{f32(double(points[0].x) - points[1].x), f32(double(points[0].y) - points[1].y),
                 f32(double(points[0].z) - points[1].z)};
    const double bx = double(points[0].x) - points[2].x, by = double(points[0].y) - points[2].y,
                 bz = double(points[0].z) - points[2].z;
    shadow.normal.x = f32(double(a.z) * by - double(a.y) * bz);
    shadow.normal.y = f32(double(a.x) * bz - double(a.z) * bx);
    shadow.normal.z = f32(double(a.y) * bx - double(a.x) * by);
    normalize(shadow.normal);
    if ((double(gravity.z) * s.up.z + double(gravity.y) * s.up.y) + double(gravity.x) * s.up.x > 0)
    {
        shadow.normal.x = -shadow.normal.x;
        shadow.normal.y = -shadow.normal.y;
        shadow.normal.z = -shadow.normal.z;
    }
    shadow.position.x = f32(double(f32(double(shadow.normal.x) * -45)) + shadow.position.x);
    shadow.position.y = f32(double(f32(double(shadow.normal.y) * -45)) + shadow.position.y);
    shadow.position.z = f32(double(f32(double(shadow.normal.z) * -45)) + shadow.position.z);
    const Vec3 from_light{f32(double(s.position.x) - light.x), f32(double(s.position.y) - light.y),
                          f32(double(s.position.z) - light.z)};
    const double sx = double(shadow.position.x) - light.x, sy = double(shadow.position.y) - light.y,
                 sz = double(shadow.position.z) - light.z;
    const double numerator = (double(from_light.z) * from_light.z + double(from_light.y) * from_light.y) +
                             double(from_light.x) * from_light.x;
    const double ratio = numerator / ((sz * sz + sy * sy) + sx * sx);
    const double square = ratio * ratio;
    shadow.factor_392c = f32(double(std::bit_cast<float>(std::uint32_t{0x3f99999a})) / square);
    const double fourth = square * square;
    shadow.factor_3930 = f32((fourth * fourth) * 112);
}
} // namespace sgn::retail
