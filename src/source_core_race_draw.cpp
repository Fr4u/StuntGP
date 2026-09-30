#include "sgn/source_core_race_draw.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_presentation.hpp"
#include "sgn/host_wide_race_view.hpp"
#include "sgn/source_texture_motion.hpp"
#include "sgn/source_camera_frame_consumers.hpp"
#include "sgn/source_world_lighting.hpp"
#include "sgn/source_world_frame_orchestration.hpp"
#include "sgn/source_world_polygon_clipping.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_race_status_text.hpp"
#include "sgn/retail_score_popup.hpp"
#include "sgn/retail_frame_border.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include "sgn/retail_pause_age_fade.hpp"
#include "sgn/retail_viewport.hpp"
#include "sgn/retail_projection.hpp"

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
void admit(const std::shared_ptr<RaceSessionPlatform> &platform, W site)
{
    if (!platform || !platform->source_construction_only_entry())
        throw SourceAllocationError(site, "race draw requires its actual source platform");
    platform->require_owner_thread();
}
void admit(SourceCoreRaceDrawContext &c, W site)
{
    admit(c.frontend.shared_platform(), site);
    c.frames.require_source_frontend(c.frontend);
}
W current(RaceSessionState &s, W site)
{
    const auto i = s.controls.current;
    if (i >= s.controls.controllers.size())
        throw SourceAllocationError(site, "race draw current controller outside owner");
    return i;
}
LoadingFrameWorld loading(SourceCoreRaceDrawContext &c)
{
    auto frame = c.frames.borrow_loading_frame();
    auto &s = c.frontend.world();
    if (&frame.submission != &c.submission || frame.quads.descriptor.data() != s.ui.descriptor.data() ||
        &frame.quads.selected_material != &s.ui.selected_material || &frame.quads.viewport != &s.viewport ||
        &frame.render_counter != &s.outer.render_counter || &frame.scene_active != &s.outer.device.scene_active ||
        &frame.finalized != &s.outer.device.finalized)
        throw FormatError("source race draw borrows foreign frame/scratch owners");
    return frame;
}
TextSubmission text(SourceCoreRaceDrawContext &c)
{
    const auto frame = loading(c);
    auto &s = c.frontend.world();
    return {s.ui.fonts,
            s.ui.selected_material,
            c.submission.specular_enabled,
            frame.quads.materials,
            s.view_quality.second,
            s.view_quality.legacy_driver_restriction,
            s.viewport};
}
void required(SourceCoreRaceDrawContext &c, SourceCoreRaceDrawChild child, W site)
{
    c.required.perform(child, site, c);
}
void screen(SourceCoreRaceDrawContext &c)
{
    const auto frame = loading(c);
    auto &s = c.frontend.world();
    std::array<PolygonSubmissionVertex, 8> local{}; // source local8; opaque UV1 host initialization only
    auto owner = lock_screen_polygon_owner(c.frontend.shared_platform());
    (void)submit_screen_polygons_432f60({live_screen_polygon_const_reader(owner), owner.count_word(),
                                         s.ui.selected_material, frame.quads.materials, local, s.viewport},
                                        c.sink);
}
} // namespace
void SourceCoreRaceDrawUnavailable::perform(SourceCoreRaceDrawChild child, W site, SourceCoreRaceDrawContext &c)
{
    admit(c, site);
    throw SourceCoreRaceDrawBoundary(
        site, static_cast<W>(child),
        "required source race draw child is not bound; native-world substitution is forbidden");
}
void prepare_source_core_world_42e870(const std::shared_ptr<RaceSessionPlatform> &platform,
                                      LightGammaDevice *const &gamma)
{
    admit(platform, 0x42e870);
    auto &s = platform->state();
    apply_requested_viewport(s.viewport);                                        // 42E870..42E87F, actual selector gate
    update_world_view_quality(s.view_quality, s.impact_particles.rate_selector); // 42E884
    reset_view_matrix_cursor(s.view_matrices);                                   // 42E889
    advance_source_texture_motion_425ad0(platform);                              // 42E88E
    prepare_source_camera_view_402e90(platform); // 42E893: SECOND view, after matrix reset
    host_wide::race_camera_published(s);         // NEW R20 host Hor+ (no-op outside an active race view scope)
    {
        const auto i = current(s, 0x42e898);
        const LightCameraView view{s.controls.controllers[i].camera,
                                   s.camera_lifecycle[i],
                                   s.camera_presentation[i],
                                   s.overlays[i],
                                   &s.view_matrices,
                                   &s.shared_model_identity};
        {
            SGN_PROFILE_SCOPE("draw.light_42dcf0");
            prepare_source_animated_light_view_42dcf0(platform, view, gamma);
        } // 42E898
    }
    reset_world_polygon_vertex_cursors(s.polygon_frame); // 42E89D
    {
        auto &scene = platform->source_scene_resources();
        const auto i = current(s, 0x42e8a2);
        auto &lifecycle = s.camera_lifecycle[i];
        const ProjectionParameters projection{s.frame_projection[0], s.frame_projection[5], s.frame_projection[10],
                                              s.frame_projection[14], lifecycle.clip_near};
        auto animation = borrow_source_track_animation_owners(s);
        SGN_PROFILE_SCOPE("draw.world_frame_42b040");
        produce_source_world_frame_42b040({scene.models(), scene, s.model_draws, s.sprite_packets,
                                           s.controls.controllers[i].camera, s.camera_presentation[i], lifecycle,
                                           s.view_matrices, s.shared_model_identity, projection, s.polygon_frame,
                                           s.frame_world_view, animation, s.view_quality.first, s.view_quality.second,
                                           s.impact_particles.rate_selector, s.split_setting, s.level.ground_level,
                                           s.scene_mode}); // 42E8A2; actual PMD, sprite and captured strip descriptors
    }
    host_presentation::pass_yield_point(); // NEW HOST (R31): may present one interpolated frame, no source work
    {
        SGN_PROFILE_SCOPE("draw.clip_4277d0");
        clip_source_world_polygon_lists_4277d0(platform);
    } // 42E8A7
    host_presentation::pass_yield_point(); // NEW HOST (R31): may present one interpolated frame, no source work
    {
        SGN_PROFILE_SCOPE("draw.project_429a40");
        project_source_world_polygon_vertices_429a40(platform);
    } // 42E8AC
    s.polygon_frame.frame_stamp = (s.polygon_frame.frame_stamp + 1u) & 0xffffu; // 42E8B1..BC only after projection
}
SourceCoreRaceViewKind draw_source_core_view_4425d0(SourceCoreRaceDrawContext &c)
{
    admit(c, 0x4425d0);
    auto &s = c.frontend.world();
    if (s.hud_block != 0)
    { // 4425D0; controller/vehicle fields remain lazy
        const auto i = current(s, 0x4425d9);
        const auto &control = s.controls.controllers[i];
        if (control.active == 0)
        {
            const auto vehicle = control.vehicle;
            if (vehicle >= s.vehicles.startup.size())
                throw SourceAllocationError(0x4425e9, "status vehicle pointer outside source owner");
            if (s.vehicles.startup[vehicle].fields_768c[5] == 1)
            {
                draw_race_finished_status_442520(text(c), c.sink);
                return SourceCoreRaceViewKind::race_finished;
            }
            draw_no_player_status_442590(text(c), c.sink);
            return SourceCoreRaceViewKind::no_player;
        }
    }
    {
        // NEW R20 HOST (wide_3d): Hor+ for the 3D stages 4425FC..44260B of this view.
        //  Widened 61C3E8/61C3FC/planes are restored before the 2D stages below;
        //  inactive (identity) with --aspect-4x3 or an exactly 4:3 output.
        host_wide::RaceViewScope wide(s);
        {
            SGN_PROFILE_SCOPE("draw.view_402e90");
            prepare_source_camera_view_402e90(c.frontend.shared_platform());
        } // 4425FC: FIRST view
        host_wide::race_camera_published(s); // NEW R20 host Hor+
        {
            SGN_PROFILE_SCOPE("draw.entities_442500");
            required(c, SourceCoreRaceDrawChild::entities, 0x442601);
        }
        host_presentation::pass_yield_point(); // NEW HOST (R31): may present one interpolated frame, no source work
        {
            SGN_PROFILE_SCOPE("draw.world_42e870");
            prepare_source_core_world_42e870(c.frontend.shared_platform(), c.gamma_publication);
        } // 442606
        {
            SGN_PROFILE_SCOPE("draw.world_submission_433f40");
            // NEW HOST (interp R19): read-only view/pose snapshot and vertex region tags.
            host_presentation::note_world_view(s);
            host_presentation::RegionScope world(host_presentation::Region::world3d);
            required(c, SourceCoreRaceDrawChild::world_submission, 0x44260b);
        }
        wide.restore(); // NEW R20: original words back before 442610 (also on exceptions, via the destructor)
    }
    host_presentation::pass_yield_point(); // NEW HOST (R31): may present one interpolated frame, no source work
    reset_screen_material_cache_42e420(s.ui.selected_material); // 442610
    {
        SGN_PROFILE_SCOPE("draw.overlay_442a00"); // NEW HOST (R30) profiler marker only
        auto frame = loading(c);
        draw_loading_overlay_before_442a00(frame.quads, c.sink);
    } // 442615
    {
        SGN_PROFILE_SCOPE("draw.hud_44ae40");
        required(c, SourceCoreRaceDrawChild::hud, 0x44261a);
    }
    host_presentation::pass_yield_point(); // NEW HOST (R31): may present one interpolated frame, no source work
    {
        SGN_PROFILE_SCOPE("draw.overlay_442a20"); // NEW HOST (R30) profiler marker only
        auto frame = loading(c);
        draw_loading_overlay_after_442a20(frame.quads, c.sink);
    } // 44261F
    {
        SGN_PROFILE_SCOPE("draw.popups_401b00"); // NEW HOST (R30) profiler marker only
        auto frame = loading(c);
        submit_score_popups_401b00({s.score_popups, s.controls, frame.quads.vertices, s.ui.selected_material,
                                    frame.quads.materials, s.viewport},
                                   c.sink);
    } // 442624
    {
        SGN_PROFILE_SCOPE("draw.screen_view");
        host_presentation::RegionScope projected(host_presentation::Region::screen3d); // NEW R19 host tag
        screen(c);
    } // 442629, including its source count==0 tail
    {
        SGN_PROFILE_SCOPE("draw.border_431830"); // NEW HOST (R30) profiler marker only
        draw_frame_border_431830({s.viewport, c.submission, s.ui.selected_material, s.view_quality.second,
                                  s.view_quality.legacy_driver_restriction},
                                 0, c.sink); // 442630 tail
    }
    return SourceCoreRaceViewKind::normal;
}
std::uint32_t draw_source_core_views_442640(SourceCoreRaceDrawContext &c)
{
    admit(c, 0x442640);
    auto &s = c.frontend.world();
    W count = 0;
    const auto view = [&](std::int32_t selector) {
        select_viewport(s.viewport, selector); // 424C30
        (void)draw_source_core_view_4425d0(c);
        ++count;
    };
    const auto split = s.split_setting; // 442640, capture ONCE; unsigned >4 runs suffix
    switch (split)
    {
    case 0:
        view(1);
        break; // does not overwrite the entry controller
    case 1:
        s.controls.current = 0;
        view(6);
        s.controls.current = 1;
        view(7);
        break;
    case 2:
        s.controls.current = 0;
        view(8);
        s.controls.current = 1;
        view(9);
        break;
    case 3:
        s.controls.current = 0;
        view(10);
        s.controls.current = 1;
        view(11);
        s.controls.current = 2;
        view(12);
        break;
    case 4:
        s.controls.current = 0;
        view(13);
        s.controls.current = 1;
        view(14);
        s.controls.current = 2;
        view(15);
        s.controls.current = 3;
        view(16);
        break;
    default:
        break;
    }
    select_viewport(s.viewport, 1);               // 442765, even after split0
    contact::player::restore_primary(s.controls); // 44276A ->402580
    s.selected_hud = current(s, 0x44276f);        // 44276F..442779, canonical current+210 identity
    {
        SGN_PROFILE_SCOPE("draw.pause_results_countdown_demo");
        required(c, SourceCoreRaceDrawChild::pause_menu, 0x44277e);
        required(c, SourceCoreRaceDrawChild::results, 0x442783);
        required(c, SourceCoreRaceDrawChild::countdown, 0x442788);
        required(c, SourceCoreRaceDrawChild::demo, 0x44278d);
    }
    // No fresh scene/pause gate or controller restoration after synchronous menu.
    {
        SGN_PROFILE_SCOPE("draw.age_fade_442200");                      // NEW HOST (R30) profiler marker only
        draw_pause_age_fade_442200(c.frames.borrow_age_fade(), c.sink); // 442792 tail
    }
    return count;
}
SourceCoreRaceDrawKind draw_source_core_race_4427b0(SourceCoreRaceDrawContext &c)
{
    admit(c, 0x4427b0);
    auto &s = c.frontend.world();
    if (s.scene_mode != 1)
        return SourceCoreRaceDrawKind::not_race;
    // NEW HOST (R31): may present one interpolated frame (442810 returned), no source work.
    host_presentation::pass_yield_point();
    ++s.outer.render_counter; // 4427B9, modulo32 before literal source gate
    // 4427BF calls4421F0, whose COMPLETE pinned body is XOR EAX,EAX;RET.
    // 4427C7's EAX==1 branch is therefore never taken; no missing call is skipped.
    {
        // NEW HOST (R30) profiler marker: the Flip, which (unless the high-refresh presenter suppresses it)
        // records the previous frame's GPU work and Presents inside this pass.
        SGN_PROFILE_SCOPE("draw.flip_432a60");
        host_presentation::RaceFlipScope flip;
        prepare_loading_surface_432a60(c.device);
    } // 4427C9 (NEW R19: host Present may be suppressed, Flip unchanged)
    {
        SGN_PROFILE_SCOPE("draw.begin_scene_433ee0");                      // NEW HOST (R30) marker: opens the capture
        begin_loading_scene_433ee0(s.outer.device.scene_active, c.device); // 4427CE
    }
    host_presentation::begin_race_draw(*c.frontend.shared_platform()); // NEW R19 host tag, no source write
    if (s.viewport.cropped == 1 || s.split_setting != 0)
    {                                           // live reads AFTER API calls
        SGN_PROFILE_SCOPE("draw.clear_4328c0"); // NEW HOST (R30) profiler marker only
        auto frame = loading(c);
        clear_loading_frame_4328c0(frame, 0, c.sink, c.device); // 4427E7
    }
    (void)draw_source_core_views_442640(c);                     // 4427EC
    reset_screen_material_cache_42e420(s.ui.selected_material); // 4427F1
    {
        SGN_PROFILE_SCOPE("draw.screen_final");
        host_presentation::RegionScope projected(host_presentation::Region::screen3d); // NEW R19 host tag
        screen(c);
    } // 4427F6 mandatory even if view already consumed count
    {
        SGN_PROFILE_SCOPE("draw.finalize_431a80");
        finalize_loading_frame_431a80(s.outer.device.finalized, c.device);
    } // 4427FB
    host_presentation::pass_yield_point(); // NEW HOST (R31): may present one interpolated frame, no source work
    {
        SGN_PROFILE_SCOPE("draw.end_scene_433f20"); // NEW HOST (R30) marker: seals and publishes the capture
        end_loading_scene_433f20(s.outer.device.scene_active, c.device); // 442800 tail
    }
    return SourceCoreRaceDrawKind::rendered;
}
} // namespace sgn::retail
