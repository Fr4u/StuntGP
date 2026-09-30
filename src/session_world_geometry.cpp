#include "sgn/session_world_geometry.hpp"
#include "sgn/session_world_join.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_world_frame_producer.hpp"
#include "sgn/source_world_frame_orchestration.hpp"
#include "sgn/source_world_polygon_clipping.hpp"

namespace sgn::retail
{
namespace
{
struct LightCursorPublication
{
    LightVertexCursor &cursor;
    std::uint32_t &owner;
    ~LightCursorPublication() noexcept
    {
        owner = cursor.offset;
    }
};
} // namespace
void finish_session_world_geometry(RaceSession &session, SessionWorldGeometryAudit audit)
{
    // New host preflight, before any recovered store. This ticket does not
    // extend level resource lifetime.
    session.shared_platform()->require_owner_thread();
    auto &state = session.state();
    auto &frame = state.polygon_frame;
    const auto ticket = capture_world_polygon_level(frame);
    validate_world_polygon_level(frame, ticket);
    if (!frame.allocations && !frame.transformed.allocation.source_bound())
        throw FormatError("Session world geometry frame allocation is absent");
    const auto current = state.controls.current;
    if (current >= state.controls.controllers.size() || current >= state.camera_presentation.size() ||
        current >= state.camera_lifecycle.size())
        throw FormatError("Session world geometry current camera is outside its owner bank");
    if (state.scene_mode != 1)
        throw FormatError("Session world geometry requires the normal world scene");
    if (session.shared_platform()->source_construction_only_entry())
    {
        // Source entry has its own actual owners. Do not enter the preview
        // Models::at/track_normal_view preflight below. The existing lease
        // must already have been attached by the completed level owner.
        auto &source_scene = session.shared_platform()->source_scene_resources();
        source_scene.require_sprite_binding(state.model_draws, state.sprite_packets);
        auto animations = borrow_source_track_animation_owners(state, audit.track);
        auto &source_camera = state.controls.controllers[current].camera;
        auto &source_lifecycle = state.camera_lifecycle[current];
        const ProjectionParameters source_projection{state.frame_projection[0], state.frame_projection[5],
                                                     state.frame_projection[10], state.frame_projection[14],
                                                     source_lifecycle.clip_near};
        try
        {
            reset_world_polygon_vertex_cursors(frame);
            produce_source_world_frame_42b040(
                {source_scene.models(), source_scene, state.model_draws, state.sprite_packets, source_camera,
                 state.camera_presentation[current], source_lifecycle, state.view_matrices, state.shared_model_identity,
                 source_projection, frame, state.frame_world_view, animations, state.view_quality.first,
                 state.view_quality.second, state.impact_particles.rate_selector, state.split_setting,
                 state.level.ground_level, state.scene_mode});
            if (audit.world_join)
                audit.world_join->preclip(session);
            clip_source_world_polygon_lists_4277d0(session.shared_platform());
            project_source_world_polygon_vertices_429a40(session.shared_platform());
            frame.frame_stamp = (frame.frame_stamp + 1u) & 0xffffu; // 42E8B1..42E8BC, only after all projection stores
            return; // submission remains a separate downstream stage, not a native fallback
        }
        catch (...)
        {
            detach_world_polygon_level(frame);
            throw;
        }
    }
    const auto track = session.scene_resources().model_handle(3), sky = session.scene_resources().model_handle(2);
    if (!track || !sky)
        throw FormatError("Session world geometry requires the loaded track and sky");
    if (!state.track_part_animations)
        throw FormatError("Session world geometry track animation owner is absent");
    if (&session.scene_resources().strip_geometry() != &state.strip)
        throw FormatError("Session world geometry strip registry names another owner");
    session.scene_resources().require_sprite_binding(state.model_draws, state.sprite_packets);
    auto &model = session.models().at(*track);
    (void)session.models().at(*sky);
    if (state.model_draws.background.records[0].model_identity != session.models().identity(track) ||
        state.model_draws.background.records[1].model_identity != session.models().identity(sky))
        throw FormatError("Session world geometry queue does not name its loaded track and sky");
    if (!model.source_outer_transform())
        throw FormatError("Session world geometry prior track pose is absent");
    if (state.light_vertices.size() != 0x4800 || state.light_vertex_offset > state.light_vertices.size())
        throw FormatError("Session world geometry lighting workspace is invalid");
    auto normals = session.track_normal_view();
    auto &camera = state.controls.controllers[current].camera;
    auto &presentation = state.camera_presentation[current];
    auto &lifecycle = state.camera_lifecycle[current];
    const ProjectionParameters projection{state.frame_projection[0], state.frame_projection[5],
                                          state.frame_projection[10], state.frame_projection[14], lifecycle.clip_near};
    LightVertexCursor lights{state.light_vertices, state.light_vertex_offset};
    const LightCursorPublication publish{lights, state.light_vertex_offset};
    try
    {
        reset_world_polygon_vertex_cursors(frame); // 42E89D ->42E800
        produce_world_frame({session.models(),
                             *track,
                             *sky,
                             state.model_draws,
                             state.sprite_packets,
                             state.strip,
                             camera,
                             presentation,
                             lifecycle,
                             state.view_matrices,
                             state.shared_model_identity,
                             projection,
                             frame,
                             state.frame_world_view,
                             state.track_part_animations->records(),
                             normals,
                             state.random,
                             state.effect_particles,
                             state.gravity,
                             {state.lights, state.light_selection, lights, state.ambient_light, state.color_tables,
                              state.split_setting, frame.frame_stamp, frame.lighting_stride},
                             state.view_quality.first,
                             state.view_quality.second,
                             state.impact_particles.rate_selector,
                             state.level.ground_level,
                             state.scene_mode,
                             &session.scene_resources()},
                            audit.track);
        if (audit.world_join)
            audit.world_join->preclip(session);
        clip_world_polygon_lists(world_polygon_clip_lists(frame), frame.clip, lifecycle.clip_near, lifecycle.clip_far,
                                 audit.clipping);
        const auto clipped_count = frame.clip.vertex_count.count_from(frame.clip.vertices);
        if (clipped_count > frame.clip.vertices.size())
            throw FormatError("Session world geometry projection cursor exceeds its allocation");
        project_world_polygon_vertices(world_polygon_indexed_projection_vertices(frame),
                                       frame.clip.vertices.first(clipped_count),
                                       world_polygon_remaining_vertices(frame), state.viewport);
        frame.frame_stamp = (frame.frame_stamp + 1u) & 0xffffu; // 42E8B1..42E8BC
    }
    catch (...)
    {
        // New host failure policy. All aliases of the same existing level
        // lease become invalid, including tickets held by presentation code.
        detach_world_polygon_level(frame);
        throw;
    }
}
} // namespace sgn::retail
