#include "sgn/retail_race_grid.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
struct Pose
{
    VehicleVector position;
    CameraVector forward;
};
} // namespace
bool initialize_race_grid(VehicleStorage &storage, VehicleResetWorld &reset, const SplineSet &splines,
                          VehicleGridState &grid, std::uint32_t mode, VehicleGridPayloadAudit *audit)
{
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto &view = reset.vehicles[i];
        if (&view.physics != &storage.physics[i] || &view.race != &storage.race[i] ||
            &view.contact != &storage.contact[i] || view.id != i)
            throw std::invalid_argument("race grid reset view must borrow actual vehicle owners");
    }
    if (!grid.start_spline)
        grid.start_spline = splines.find(1);
    if (!grid.start_spline)
        return false;
    if (mode == 1)
        return initialize_vehicle_starting_grid(storage, reset, splines, grid, audit);
    if (mode != 2)
        return false;
    const auto route = splines.find(2);
    if (!route)
        throw FormatError("race grid mode2 requires authored spline2");
    std::array<std::optional<Pose>, 6> poses;
    for (unsigned i = 0; i < 6; ++i)
    {
        if (storage.race[i].active != 1)
            continue;
        const auto product = route->descriptor.point_count * (87u - 2 * i);
        const auto index = product / 100u;
        const auto start = spline_point(*route, index).value, next = spline_point(*route, index + 1).value;
        CameraVector forward{f32(double(next.x) - start.x), f32(double(next.y) - start.y),
                             f32(double(next.z) - start.z), 0};
        camera_normalize(forward);
        poses[i] = Pose{start, forward};
    }
    auto contacts = storage.contact_views();
    reset.current = 0;
    for (unsigned i = 0; i < 6; ++i)
    {
        reset.current = i;
        if (!poses[i])
            continue;
        auto &physics = storage.physics[i];
        const auto &pose = *poses[i];
        reset_vehicle_motion(physics);
        restart_vehicle(reset, i, storage.restart[i]);
        physics.velocity = {};
        physics.acceleration = {};
        const VehicleVector right{f32(0.0 - pose.forward.z), 0, f32(double(pose.forward.x) - 0.0),
                                  physics.orientation[3]};
        const VehicleVector up{0, 1, 0, physics.orientation[7]};
        const VehicleVector forward{pose.forward.x, pose.forward.y, pose.forward.z, physics.orientation[11]};
        physics.orientation = vehicle_basis(right, up, forward);
        storage.contact[i].body.orientation = physics.orientation;
        const double offset = i % 2 == 0 ? 300.0 : -300.0; // pinned six records at4744F0
        const float x = f32((0.0 * forward.x + 0.0 * up.x) + double(right.x) * offset);
        const float y = f32((0.0 * up.y + 0.0 * forward.y) + double(right.y) * offset);
        const float z = f32((0.0 * up.z + 0.0 * forward.z) + double(right.z) * offset);
        const float px = f32(double(x) + pose.position.x), py = f32(double(y) + pose.position.y),
                    pz = f32(double(z) + pose.position.z);
        contact::solver::place(contacts[i],
                               {px, f32(double(py) + 150.0), pz, std::bit_cast<std::uint32_t>(physics.position.w)});
        if (audit)
        {
            audit->orientation[i] = true;
            audit->position[i] = true;
        }
    }
    reset.current = 0;
    return true;
}
} // namespace sgn::retail
