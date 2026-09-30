#include "sgn/retail_camera_frame.hpp"
#include "sgn/retail_camera_transform.hpp"
#include "sgn/impact_particle_lifetime.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/retail_camera_spline.hpp"
#include "sgn/retail_camera_mounted.hpp"
#include "sgn/model_resources.hpp"
#include "camera_mounted_provider.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
void check_current(const CameraFrameWorld &world)
{
    if (world.controls.current >= 4)
        throw FormatError("camera frame controller outside four slots");
}
void publish(CameraFrameWorld &world, const CameraState &camera)
{
    world.frame_projection = camera.projection;
    world.inverse_depth_scale = camera.inverse_depth_scale;
    world.depth_offset = camera.depth_offset;
}
CameraEffectTargets effect_targets(CameraFrameWorld &world)
{
    CameraEffectTargets result;
    for (unsigned i = 0; i < 6; ++i)
    {
        result.vehicles[i] = &world.vehicles.physics[i];
        result.active[i] = static_cast<std::uint32_t>(world.vehicles.race[i].active);
    }
    return result;
}
CameraVector solve_collision(void *context, const CameraSweep &sweep)
{
    auto &world = *static_cast<CameraFrameWorld *>(context);
    auto &query = world.contact_cast;
    auto &work = world.contact_workspace;
    // Invoked only after403460's mounted/bypass/distance guards. Preserve the
    // original query ownership, including40351E's mass write and velocity W.
    query.start = {sweep.position.x, sweep.position.y, sweep.position.z,
                   std::bit_cast<std::uint32_t>(sweep.position.w)};
    work.query_mass = 1;
    work.query_motion = {sweep.velocity.x, sweep.velocity.y, sweep.velocity.z};
    query.motion_payload = std::bit_cast<std::uint32_t>(sweep.velocity.w);
    query.radius = 125;
    query.radius_squared = 15625;
    query.remaining = 1;
    query.travel_limit = vehicle_f32(
        std::sqrt((double(sweep.velocity.z) * sweep.velocity.z + double(sweep.velocity.y) * sweep.velocity.y) +
                  double(sweep.velocity.x) * sweep.velocity.x));
    for (unsigned i = 0; i < 33; ++i)
    {
        if (contact::effects::cast_track_particle_sphere(world.track, work, query) != 1)
            break;
        contact::reflect_motion({work.last_normal.x, work.last_normal.y, work.last_normal.z}, 0.01f, work.query_motion);
    }
    return {query.end.x, query.end.y, query.end.z, std::bit_cast<float>(query.end.opaque)};
}
CameraRaceContext race_context(const CameraFrameWorld &world, const std::array<CameraVehicle, 6> &vehicles)
{
    const auto i = world.current_vehicle;
    auto selected = i;
    // 404360 uses signed INC/CMP for the rank index, and reads ranking only
    // when adaptive distance is enabled. Invalid source pointers are rejected.
    if (vehicles[i].adaptive_distance)
    {
        if (world.ranking.count > 6)
            throw FormatError("camera ranking count exceeds six cars");
        const auto next = std::bit_cast<std::int32_t>(world.vehicles.race[i].place + 1u);
        if (next < std::bit_cast<std::int32_t>(world.ranking.count))
        {
            if (next < 0 || !world.ranking.slots[static_cast<unsigned>(next)])
                throw FormatError("camera has invalid next ranked vehicle");
            selected = *world.ranking.slots[static_cast<unsigned>(next)];
            if (selected >= 6)
                throw FormatError("camera next ranked slot outside six cars");
        }
    }
    return {vehicles[selected].progress, vehicles[selected].speed, world.clock.course_length};
}
void prepare_view(CameraFrameWorld &world, CameraPathInput input)
{
    auto targets = effect_targets(world);
    camera_prepare_view(world.controls, world.lifecycle, world.presentation, world.session, world.splines, input,
                        camera_vehicle_reset_adapter(targets), world.shared_model_identity, &world.matrix_arena);
    publish(world, world.controls.controllers[world.controls.current].camera);
}
} // namespace
CameraFrameSnapshot::CameraFrameSnapshot(const CameraState &camera, const CameraPresentationState &presentation,
                                         const CameraLifecycle &life, std::uint32_t controller,
                                         std::uint32_t active_lights)
    : camera_(camera), world_to_view_(camera_world_view(camera)), planes_(presentation.planes), controller_(controller),
      active_lights_(active_lights), near_(life.clip_near), far_(life.clip_far)
{
}
void camera_frame_begin_outer(CameraFrameWorld &world)
{
    camera_begin_outer_frame(world.controls);
}
void camera_frame_before_world(CameraFrameWorld &world)
{
    camera_frame_before_world(world, nullptr);
}
void camera_frame_before_world(CameraFrameWorld &world, CameraMountedAudit *audit)
{
    auto targets = effect_targets(world);
    const auto reset = camera_vehicle_reset_adapter(targets);
    const CameraCollision collision{&world, solve_collision};
    const auto &unit = world.gravity.unit;
    const CameraVector gravity{unit.x, unit.y, unit.z, unit.w};
    // 405218 publishes each current controller before its exact-one active gate.
    for (unsigned i = 0; i < 4; ++i)
    {
        world.controls.current = i;
        auto &c = world.controls.controllers[i];
        if (c.active != 1)
            continue;
        if (c.vehicle >= 6)
            throw FormatError("active camera target outside six cars");
        std::array<CameraVehicle, 6> observed;
        for (unsigned slot = 0; slot < 6; ++slot)
            observed[slot] = world.vehicles.camera_snapshot(slot);
        auto &camera = c.camera;
        auto &life = world.lifecycle[i];
        auto &vehicle = observed[c.vehicle];
        const bool record_write = life.pending_record.has_value();
        camera_apply_record(camera, life, vehicle, world.bank, world.template_pose, world.shared_model_identity);
        if (record_write)
            publish(world, camera);
        if (camera.mode == 10)
        {
            camera_prepare_authored(camera, life, vehicle, world.bank);
            world.vehicles.store_camera_settings(c.vehicle, vehicle);
        }
        else
        {
            const bool prepare_write = camera.mode >= 11 && camera.mode <= 16 && camera.pending_follow_setup == 1;
            const bool preset_write = prepare_write && camera.mode <= 15;
            camera_prepare_follow_mode(camera, vehicle, world.selected_preset, reset, world.shared_model_identity);
            if (preset_write)
                publish(world, camera);
            if (prepare_write)
                world.vehicles.store_camera_settings(c.vehicle, vehicle);
        }
        const bool lens_write = camera.pending_lens != 0;
        camera_consume_lens(camera, world.shared_model_identity);
        if (lens_write)
            publish(world, camera);
        if (camera.mode == 4 || camera.mode == 5 || (camera.mode >= 11 && camera.mode <= 16))
        {
            if (world.current_vehicle >= 6)
                throw FormatError("camera current vehicle outside six cars");
            const auto race = camera.mode == 4 ? CameraRaceContext{} : race_context(world, observed);
            camera_chase_tick(camera, vehicle, race, gravity, collision, world.shared_model_identity,
                              &observed[world.current_vehicle]);
            world.vehicles.store_camera_settings(c.vehicle, vehicle);
            if (world.current_vehicle != c.vehicle)
                world.vehicles.store_camera_settings(world.current_vehicle, observed[world.current_vehicle]);
            if (camera.mode == 5)
                publish(world, camera);
        }
        else if (camera.mode == 6)
        {
            struct Context
            {
                CameraFrameWorld &world;
                CameraMountedAudit *audit;
            } context{world, audit};
            const detail::MountedCameraProvider provider{
                &context,
                [](void *p, CameraVector value) {
                    auto &w = static_cast<Context *>(p)->world;
                    auto &orbit =
                        w.vehicles.camera_settings[w.controls.controllers[w.controls.current].vehicle].orbit_offset;
                    orbit.x = value.x;
                    orbit.y = value.y;
                    orbit.z = value.z; // 404D09..404D19; live sole owner, W retained
                },
                [](void *p, std::uint32_t site) {
                    auto &context = *static_cast<Context *>(p);
                    auto &w = context.world;
                    if (context.audit && context.audit->before_axis)
                        context.audit->before_axis(context.audit->context, site);
                    // Fresh chain for each axis. Nothing here is prefetched for another controller.
                    const auto handle = w.vehicles.models[w.controls.controllers[w.controls.current].vehicle].model;
                    if (!w.models || !handle)
                        throw FormatError("mounted camera has no live source model");
                    return detail::mounted_axis(*w.models, *handle, site);
                }};
            detail::camera_mounted_core(camera, vehicle, provider);
        }
        else if (camera.mode == 7)
        {
            if (!world.presentation[i].spline)
                throw FormatError("spline chase has no authored source route");
            camera_spline_chase_tick(camera, vehicle, world.session.path_parameter, *world.presentation[i].spline,
                                     gravity, collision);
        }
        else
        {
            camera.previous_target = camera.target;
            camera_move(camera, vehicle, collision);
            camera_translate(camera, vehicle);
        }
        camera_integrate_velocity(camera);
        camera_integrate_angles(camera);
    }
    world.controls.current = 0; // 40524F, reached only after all controllers complete
}
CameraFrameSnapshot camera_frame_snapshot(const CameraFrameWorld &world)
{
    check_current(world);
    const auto index = world.controls.current;
    auto camera = world.controls.controllers[index].camera;
    camera.camera_to_world_rotation = resolve_arena_matrix(world.matrix_arena, camera.camera_to_world_reference);
    camera.world_to_camera_rotation = resolve_arena_matrix(world.matrix_arena, camera.world_to_camera_reference);
    return {camera, world.presentation[index], world.lifecycle[index], index, world.light_selection.active_count};
}
CameraFrameSnapshot camera_frame_normal_views(CameraFrameWorld &world, CameraPathInput input, CameraFrameStages &stages,
                                              LightGammaDevice *gamma)
{
    check_current(world);
    if (world.light_vertices.size() != 0x4800 || world.light_vertex_offset > world.light_vertices.size())
        throw FormatError("camera frame light buffer outside source allocation");
    prepare_view(world, input);
    stages.stage_entities_442500(camera_frame_snapshot(world));
    apply_requested_viewport(world.viewport); // 42E870..42E883, before remaining prefix
    stages.prepare_world_42e870();
    prepare_view(world, input);
    const auto selected = world.controls.current;
    LightVertexCursor cursor{world.light_vertices, world.light_vertex_offset};
    prepare_animated_light_view(world.lights, world.light_selection, world.light_fade, world.light_animation,
                                {world.controls.controllers[selected].camera, world.lifecycle[selected],
                                 world.presentation[selected], world.overlays[selected]},
                                cursor, world.split_setting, gamma);
    world.light_vertex_offset = cursor.offset;
    const auto snapshot = camera_frame_snapshot(world);
    stages.consume_model_views(snapshot);
    return snapshot;
}
} // namespace sgn::retail
