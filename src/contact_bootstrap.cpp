#include "sgn/contact_bootstrap.hpp"
#include <cmath>

namespace sgn::retail::contact::solver
{
void place_template_from_camera(VehicleState &vehicle, State &contact, const CameraVector &position,
                                const CameraVector &forward, VehiclePayloadAudit *audit)
{
    const auto finite = [](float x, float y, float z) {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    };
    if (contact.rig.count > contact.rig.probes.size() || !finite(position.x, position.y, position.z) ||
        !finite(forward.x, forward.y, forward.z))
        throw FormatError("bootstrap camera or probe count outside recovered domain");
    for (unsigned j = 0; j < contact.rig.count; ++j)
    {
        const auto &p = contact.rig.probes[j].local;
        if (!finite(p.x, p.y, p.z))
            throw FormatError("bootstrap local probe must be finite");
    }
    // The source subtracts its own Y before normalization; camera pitch is
    // deliberately removed. Keep every source float store in its original order.
    auto direction =
        vehicle_normalize({vehicle_f32(double(forward.x) - 0.0), vehicle_f32(double(forward.y) - forward.y),
                           vehicle_f32(double(forward.z) - 0.0), forward.w});
    direction.x = vehicle_f32(double(direction.x) * 2000.0);
    direction.y = vehicle_f32(double(direction.y) * 2000.0);
    direction.z = vehicle_f32(double(direction.z) * 2000.0);
    direction.y = vehicle_f32(double(direction.y) + 500.0);
    const Vec3 point{vehicle_f32(double(direction.x) + position.x), vehicle_f32(double(direction.y) + position.y),
                     vehicle_f32(double(direction.z) + position.z)};
    contact.body.position.x = point.x;
    contact.body.position.y = point.y;
    contact.body.position.z = point.z;
    for (unsigned j = 0; j < contact.rig.count; ++j)
    {
        auto &p = contact.rig.probes[j];
        p.current.x = vehicle_f32(double(p.local.x) + point.x);
        p.current.y = vehicle_f32(double(p.local.y) + point.y);
        p.current.z = vehicle_f32(double(p.local.z) + point.z);
    }
    contact.body.orientation = vehicle_identity();
    contact.body.incremental = vehicle_identity();
    contact.matrix_7338 = vehicle_identity();
    contact.body.velocity.x = 0;
    contact.body.velocity.y = 0;
    contact.body.velocity.z = 0;
    vehicle.position.x = point.x;
    vehicle.position.y = point.y;
    vehicle.position.z = point.z;
    vehicle.orientation = vehicle_identity();
    vehicle.incremental = vehicle_identity();
    vehicle.secondary_rotation = vehicle_identity();
    vehicle.velocity = {0, 0, 0};
    // All position/velocity/probe fourth words are untouched. Only these four
    // audited matrix lanes become defined through the full identity writers.
    if (audit)
    {
        audit->body_orientation_first = false;
        audit->body_incremental_first = false;
        audit->orientation_first = false;
        audit->incremental_first = false;
    }
}
} // namespace sgn::retail::contact::solver
