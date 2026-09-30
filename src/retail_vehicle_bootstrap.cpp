#include "sgn/retail_vehicle_bootstrap.hpp"
#include "sgn/contact_bootstrap.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
void require_bindings(VehicleStorage &storage, const VehicleResetWorld &reset)
{
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto &view = reset.vehicles[i];
        if (&view.physics != &storage.physics[i] || &view.race != &storage.race[i] ||
            &view.contact != &storage.contact[i] || view.id != i)
            throw std::invalid_argument("bootstrap reset view must borrow the same physical slot");
    }
}
using sgn::bit_reinterpret::as_float;
contact::Vector4 position(VehicleVector p)
{
    return {p.x, p.y, p.z, std::bit_cast<std::uint32_t>(p.w)};
}
} // namespace
bool initialize_vehicle_starting_grid(VehicleStorage &storage, VehicleResetWorld &reset, const SplineSet &splines,
                                      VehicleGridState &grid, VehicleGridPayloadAudit *audit)
{
    require_bindings(storage, reset);
    if (!grid.start_spline)
        grid.start_spline = splines.find(1); // 4130D0 ->428800
    if (!grid.start_spline)
        return false;
    std::array<std::int32_t, 6> active{};
    for (unsigned i = 0; i < 6; ++i)
        active[i] = storage.race[i].active;
    const auto poses = starting_grid(*grid.start_spline, active, reset.parameters.selector);
    auto contacts = storage.contact_views();
    reset.current = 0;
    for (unsigned i = 0; i < 6; ++i)
    {
        reset.current = i;
        if (!poses[i])
            continue;
        auto &physics = storage.physics[i];
        const auto &pose = *poses[i];
        reset_vehicle_motion(physics);                 // 413490
        restart_vehicle(reset, i, storage.restart[i]); // complete413530
        physics.velocity = {};
        physics.acceleration = {};
        const VehicleVector right{pose.basis[0].x, pose.basis[0].y, pose.basis[0].z, physics.orientation[3]};
        const VehicleVector up{pose.basis[1].x, pose.basis[1].y, pose.basis[1].z, physics.orientation[7]};
        const VehicleVector forward{pose.basis[2].x, pose.basis[2].y, pose.basis[2].z, physics.orientation[11]};
        physics.orientation = vehicle_basis(right, up, forward);
        storage.contact[i].body.orientation = physics.orientation;
        storage.race[i].place = pose.physical_rank;
        const VehicleVector p{pose.position.x, pose.position.y, pose.position.z,
                              pose.copied_position_w.value_or(physics.position.w)};
        contact::solver::place(contacts[i], position(p)); // 408640 including probe XYZ and preserved probe W
        if (audit)
        {
            audit->orientation[i] = true;
            audit->position[i] = !pose.copied_position_w.has_value();
        }
    }
    reset.current = 0;
    return true;
}
bool initialize_vehicle_bootstrap(VehicleBootstrapWorld world, const VehicleBootstrapParameters &input,
                                  VehicleGridPayloadAudit *audit)
{
    auto &storage = world.vehicles;
    require_bindings(storage, world.reset);
    if (storage.contact[0].surface.object_mode)
        throw std::domain_error("bootstrap requires cold template with null external object726C");
    world.reset.current = 0; // source current4D5BB8 and template474630 both select slot0
    auto loads = storage.load_views();
    VehicleLoadWorld loader{loads, world.reset.current};
    load_vehicle_configuration(loader, world.assets, 0, input.config_id, input.control_kind);
    auto &physics = storage.physics[0];
    auto &params = storage.parameters[0];
    physics.position.x = 0;
    physics.position.y = 0;
    physics.position.z = as_float(0xc63ea000);
    params.tyres[0].radius = as_float(0x420a0000);
    params.tyres[1].radius = as_float(0x420a0000);
    params.tyres[2].radius = as_float(0x41c40000);
    params.tyres[3].radius = as_float(0x41c40000);
    params.wheelbase = as_float(0x437a0000);
    params.track_front = as_float(0x42fd3334);
    params.track_rear = as_float(0x43340000);
    params.mass = as_float(0x447a0000);
    params.com_a = as_float(0x3f000000);
    params.com_b = as_float(0x3f000000);
    params.com_height = as_float(0x41f00001);
    reset_race_record(storage.race[0], physics);
    clear_wheel_acceleration(physics);
    contact::solver::initialize(storage.contact[0], input.cars[0].model); // 4082D0 reads slot0's 60F034
    contact::solver::place_template_from_camera(physics, storage.contact[0], input.camera_position,
                                                input.camera_forward);
    initialize_vehicle_strip(world.strip, storage.strips[0], world.strip_binding);
    auto contacts = storage.contact_views();
    for (unsigned i = 1; i < 6; ++i)
    {
        storage.clone_vehicle(0, i);
        storage.physics[i].position.x = vehicle_f32(double(storage.physics[i].position.x) + as_float(0x43fa0000));
        contact::solver::place(contacts[i], position(storage.physics[i].position));
        storage.models[i].model = world.models.resolve(input.cars[i].source_word00);
        storage.models[i].shadow = world.models.resolve(input.cars[i].source_word04);
    }
    for (unsigned i = 0; i < 6; ++i)
        if (input.cars[i].source_word00 == 0)
            storage.race[i].active = 0;                                           // full413500
    initialize_vehicle_model_anchors(storage, world.models, world.reset.current); // 413785..413798
    // 419900 is literally RET in the pinned original. It has no writes/callees.
    return initialize_vehicle_starting_grid(storage, world.reset, world.splines, world.grid, audit);
}
} // namespace sgn::retail
