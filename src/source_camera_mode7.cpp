#include "sgn/source_camera_mode7.hpp"
#include "sgn/source_camera_motion.hpp"
#include "sgn/source_camera_frame_consumers.hpp"
#include "sgn/race_session.hpp"
#include "sgn/track_contact.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cstddef>
#include <cmath>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Controller = contact::player::Controller;
float f32(double v)
{
    return static_cast<float>(v);
}
using sgn::bit_reinterpret::as_float;
bool c0(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
Controller &current(RaceSessionState &world, W site)
{
    if (world.controls.current >= world.controls.controllers.size())
        throw SourceAllocationError(site, "mode7 current camera outside actual controls bank");
    return world.controls.controllers[world.controls.current];
}
W vehicle_slot(const RaceSessionState &world, const Controller &controller, W site)
{
    const auto slot = controller.vehicle;
    if (slot >= world.vehicles.physics.size())
        throw SourceAllocationError(site, "camera vehicle pointer outside actual six-slot bank");
    return slot;
}
double remainder_one(double v)
{
    if (!std::isfinite(v))
        throw SourceAllocationError(0x46291a, "mode7 remainder requires recovered finite CRT domain");
    return std::fmod(v, 1.0);
}
void target_403770(RaceSessionState &world, W slot)
{
    // ECX is captured by404FAD, not reselected during this child.
    if (world.vehicles.race[slot].active == 0)
        return;
    const auto &physical = world.vehicles.physics[slot];
    const auto &offset = world.vehicles.camera_settings[slot].target_offset;
    const double sum_x = double(offset.x) + physical.position.x;
    const double sum_y = double(offset.y) + physical.position.y;
    auto &target = current(world, 0x403798).camera.target;
    const float y = f32(sum_y), z = f32(double(offset.z) + physical.position.z);
    const double dx = sum_x - target.x;
    const float dy = f32(double(y) - target.y), dz = f32(double(z) - target.z);
    const double sx = dx * 0.125;
    const float sy = f32(double(dy) * 0.125), sz = f32(double(dz) * 0.125);
    target.x = f32(sx + target.x);
    target.y = f32(double(sy) + target.y);
    target.z = f32(double(sz) + target.z);
}
void translate_403340(RaceSessionState &world)
{
    auto &selected = current(world, 0x403340);
    auto &camera = selected.camera;
    // EAX camera remains captured; its+AC vehicle pointer is loaded afresh for
    // each component. No CameraVehicle snapshot or cached native view is used.
    const auto physics = [&](W site) -> const VehicleState & {
        return world.vehicles.physics[vehicle_slot(world, selected, site)];
    };
    camera.position.x = f32(double(camera.position.x) - physics(0x403345).previous_auxiliary_displacement.x);
    camera.position.y = f32(double(camera.position.y) - physics(0x403357).previous_auxiliary_displacement.y);
    camera.position.z = f32(double(camera.position.z) - physics(0x403369).previous_auxiliary_displacement.z);
    camera.target.x = f32(double(camera.target.x) - physics(0x40337b).previous_auxiliary_displacement.x);
    camera.target.y = f32(double(camera.target.y) - physics(0x40338d).previous_auxiliary_displacement.y);
    camera.target.z = f32(double(camera.target.z) - physics(0x40339f).previous_auxiliary_displacement.z);
    camera.position.x = f32(double(physics(0x4033b1).auxiliary_displacement.x) + camera.position.x);
    camera.position.y = f32(double(physics(0x4033c3).auxiliary_displacement.y) + camera.position.y);
    camera.position.z = f32(double(physics(0x4033d5).auxiliary_displacement.z) + camera.position.z);
    camera.target.x = f32(double(physics(0x4033e7).auxiliary_displacement.x) + camera.target.x);
    camera.target.y = f32(double(physics(0x4033f9).auxiliary_displacement.y) + camera.target.y);
    camera.target.z = f32(double(physics(0x40340b).auxiliary_displacement.z) + camera.target.z);
}
void move_403460(RaceSessionPlatform &platform, SourceCameraMode7Contact *child)
{
    auto &world = platform.state();
    auto &selected = current(world, 0x403460);
    auto &camera = selected.camera;
    if (camera.mode == 6)
        return; // source early gate, no vehicle read
    const auto slot = vehicle_slot(world, selected, 0x403470);
    const auto &position = world.vehicles.physics[slot].position;
    const double dx = double(camera.position.x) - position.x;
    const double dy = double(camera.position.y) - position.y;
    const double dz = double(camera.position.z) - position.z;
    if ((dz * dz + dy * dy) + dx * dx > 6250000.0)
        camera.collision_bypass_ticks = 600;
    if (camera.collision_bypass_ticks != 0)
    {
        camera.position.x = f32(double(camera.velocity.x) + camera.position.x);
        camera.position.y = f32(double(camera.velocity.y) + camera.position.y);
        camera.position.z = f32(double(camera.velocity.z) + camera.position.z);
        --camera.collision_bypass_ticks;
        return;
    }
    auto &query = world.contact_cast;
    auto &work = world.contact_workspace;
    const auto px = camera.position.x, py = camera.position.y;
    query.start.x = px;
    const auto pz = camera.position.z;
    query.start.y = py;
    const auto pw = std::bit_cast<W>(camera.position.w);
    query.start.z = pz;
    query.start.opaque = pw;
    const auto vx = camera.velocity.x;
    work.query_mass = 1.0f;
    work.query_motion.x = vx;
    query.radius = 125.0f;
    const auto vy = camera.velocity.y;
    query.radius_squared = 15625.0f;
    work.query_motion.y = vy;
    query.remaining = 1.0f;
    const auto vz = camera.velocity.z;
    work.query_motion.z = vz;
    const double zz = double(work.query_motion.z) * work.query_motion.z;
    const double yy = double(work.query_motion.y) * work.query_motion.y;
    query.motion_payload = std::bit_cast<W>(camera.velocity.w);
    query.travel_limit = f32(std::sqrt((zz + yy) + double(work.query_motion.x) * work.query_motion.x));
    W iterations = 0;
    for (;;)
    {
        if (!child)
            throw SourceAllocationError(0x403597, "source camera collision requires actual PMD4215B0 receiver");
        if (child->cast_source_track_particle_sphere_4215b0(platform) != 1)
            break;
        contact::reflect_motion({work.last_normal.x, work.last_normal.y, work.last_normal.z}, as_float(0x3c23d70a),
                                work.query_motion); // 421C30, actual shared4980E8
        const auto previous = iterations++;
        if (previous >= 32)
            break; // 33 queries/reflections maximum
    }
    auto &output = current(world, 0x4035b3).camera.position;
    const auto ex = query.end.x, ey = query.end.y;
    output.x = ex;
    const auto ez = query.end.z;
    output.y = ey;
    const auto ew = query.end.opaque;
    output.z = ez;
    output.w = std::bit_cast<float>(ew);
}
} // namespace
void track_source_camera_target_403770(RaceSessionPlatform &platform, W slot)
{
    platform.require_owner_thread();
    if (slot >= platform.state().vehicles.physics.size())
        throw SourceAllocationError(0x403770, "source camera target outside actual vehicle bank");
    target_403770(platform.state(), slot);
}
void translate_source_camera_403340(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    translate_403340(platform.state());
}
void move_source_camera_403460(RaceSessionPlatform &platform, SourceCameraMode7Contact &contact)
{
    platform.require_owner_thread();
    move_403460(platform, &contact);
}
void advance_source_camera_mode7_404fad(RaceSessionPlatform &platform, SourceCameraMode7Contact *contact_child)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(0x404450, "mode7 requires actual source platform");
    auto &world = platform.state();
    auto &initial = current(world, 0x404456);
    if (initial.camera.mode != 7)
        throw SourceAllocationError(0x40447b, "bounded source camera consumer requires mode7");
    // 40445D..404478 common entry: all four previous-target words precede the
    // mode branch and its first vehicle access. They survive every later error.
    auto &camera = initial.camera;
    camera.previous_target.x = camera.target.x;
    camera.previous_target.y = camera.target.y;
    camera.previous_target.z = camera.target.z;
    camera.previous_target.w = camera.target.w;
    target_403770(world, vehicle_slot(world, initial, 0x404fad));
    auto &captured = current(world, 0x404fb8); // ESI retained through405092
    const auto slot = vehicle_slot(world, captured, 0x404fbe);
    const auto &race = world.vehicles.race[slot];
    auto &parameter = world.camera_session.path_parameter;
    double difference = double(race.progress) - parameter;
    difference -= c0(race.progress, parameter) ? -2.0 : 2.0;
    if (std::abs(difference) > as_float(0x3c23d70a))
    {
        double step = difference * as_float(0x3ca3d70a);
        if (std::abs(step) > as_float(0x3e4ccccd))
            step = c0(step, 0.0) ? as_float(0xbe4ccccd) : as_float(0x3e4ccccd);
        parameter = f32(step + parameter);
    }
    const double scaled = double(parameter) * as_float(0x3c23d70a);
    const float stored = f32(scaled); // 40505C FST: comparison retains unrounded scaled
    const double fraction = c0(scaled, 0.0) ? 1.0 - remainder_one(-double(stored)) : remainder_one(double(stored));
    // Recover the captured controller's ordinal without another global-current
    // read. Its allocation is the actual fixed controls bank for this invocation.
    const auto controller_index = static_cast<std::size_t>(&captured - world.controls.controllers.data());
    const auto *binding = world.camera_presentation[controller_index].spline.source();
    if (!binding)
        throw SourceAllocationError(0x4050a3, "mode7 reached missing actual source camera spline");
    const auto spline = *binding;
    const auto count = spline.read_word(8, 0x4050a3); // unsigned FILD QWORD
    Vec3 point;
    sample_source_camera_path_428520(spline, f32(double(count) * fraction), point);
    auto &selected = current(world, 0x4050b8);
    auto &position = selected.camera.position;
    position.x = point.x;
    position.y = point.y;
    position.z = point.z; // undefined sampled W retained
    const auto physical_slot = vehicle_slot(world, selected, 0x4050da);
    const auto vehicle_position = world.vehicles.physics[physical_slot].position; // source local16-byte copy
    const double dx = double(position.x) - vehicle_position.x;
    const double dz = double(position.z) - vehicle_position.z;
    double height = std::sqrt(dz * dz + dx * dx);
    height *= as_float(0x3dcccccd);
    height *= as_float(0x3dcccccd);
    height *= height;
    if (c0(height, 1000.0))
        height = 1000.0;
    else if (height > 3000.0)
        height = 3000.0;
    const float gx = f32(double(world.gravity.unit.x) * height);
    const float gy = f32(double(world.gravity.unit.y) * height);
    const double gz = height * world.gravity.unit.z;
    position.x = f32(double(position.x) - gx);
    position.y = f32(double(position.y) - gy);
    position.z = f32(double(position.z) - gz);
    move_403460(platform, contact_child);
    translate_403340(world);
}
} // namespace sgn::retail
