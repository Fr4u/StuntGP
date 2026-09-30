#include "sgn/session_scene_frame.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_lifecycle.hpp"
#include "sgn/retail_loading_quads.hpp"
#include "sgn/retail_race_status_text.hpp"
#include "sgn/retail_race_hud_submission.hpp"
#include "sgn/retail_score_popup.hpp"
#include "sgn/retail_frame_border.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include <utility>

namespace sgn::retail
{
SessionSceneFrameResult prepare_session_scene_frame(RaceSession &session, const SessionWorldMaterials &materials,
                                                    SessionPolygonCapabilities capabilities,
                                                    PolygonSubmissionState &scratch, PolygonSubmissionSink &sink,
                                                    SessionWorldFrameResources resources,
                                                    const RaceTextureReplacement &textures)
{
    const auto platform = session.shared_platform();
    platform->require_owner_thread();
    auto &s = session.state();
    const auto ticket = capture_world_polygon_level(s.polygon_frame);
    validate_world_polygon_level(s.polygon_frame, ticket);
    const auto &material_state = materials.validate_for(session);
    (void)materials.validate_fonts_for(session);
    auto &ui = s.ui;
    const auto text = [&] {
        return TextSubmission{ui.fonts,
                              ui.selected_material,
                              scratch.specular_enabled,
                              material_state.names.materials.active_materials(),
                              s.view_quality.second,
                              s.view_quality.legacy_driver_restriction,
                              s.viewport};
    };
    try
    {
        // 4425D0..4425F7: vehicle identity is read only in the status branch.
        if (s.hud_block != 0)
        {
            if (s.controls.current >= s.controls.controllers.size())
                throw FormatError("scene status controller outside owner");
            const auto &controller = s.controls.controllers[s.controls.current];
            if (controller.active == 0)
            {
                if (controller.vehicle >= s.vehicles.startup.size())
                    throw FormatError("scene status vehicle outside owner");
                if (s.vehicles.startup[controller.vehicle].fields_768c[5] == 1)
                {
                    draw_race_finished_status_442520(text(), sink);
                    return {SessionSceneFrameKind::race_finished, std::nullopt};
                }
                draw_no_player_status_442590(text(), sink);
                return {SessionSceneFrameKind::no_player, std::nullopt};
            }
        }
        // Host capacity is established before source work. World preparation
        // may grow/swap the same alpha vector, so acquire UI spans afterwards.
        prepare_polygon_submission_ui_capacity(scratch, resources.submission_budget);
        auto camera = prepare_session_world_frame(session, materials, capabilities, scratch, sink, resources);
        std::span<PolygonSubmissionVertex, 4> vertices{scratch.alpha_vertices.data(), 4};
        const LoadingQuadWorld loading{
            ui.descriptor,   vertices,      ui.selected_material, material_state.names.materials.active_materials(),
            s.viewport,      s.controls,    s.overlays,           ui.loading_tile_base,
            ui.loading_fade, s.frame_pause, ui.second_pause};
        reset_screen_material_cache_42e420(ui.selected_material);
        draw_loading_overlay_before_442a00(loading, sink);
        AudioDriver audio{s.audio_driver, platform->audio_device(), s.audio_level, s.controls, s.split_setting};
        submit_race_hud_44ae40({{s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                                s.vehicles,
                                s.clock,
                                s.records,
                                s.ranking,
                                s.cars,
                                s.game_text,
                                s.split_setting,
                                s.frame_pause,
                                textures,
                                ui.stars,
                                ui.descriptor,
                                vertices,
                                text(),
                                audio},
                               sink);
        draw_loading_overlay_after_442a20(loading, sink);
        submit_score_popups_401b00({s.score_popups, s.controls, vertices, ui.selected_material,
                                    material_state.names.materials.active_materials(), s.viewport},
                                   sink);
        std::array<PolygonSubmissionVertex, 8> local_screen{}; // source LOCAL8, host opaque-word initialization
        auto screen_access = lock_screen_polygon_owner(platform);
        const auto screen_result = submit_screen_polygons_432f60(
            {live_screen_polygon_const_reader(screen_access), screen_access.count_word(), ui.selected_material,
             material_state.names.materials.active_materials(), local_screen, s.viewport},
            sink);
        draw_frame_border_431830({s.viewport, scratch, ui.selected_material, s.view_quality.second,
                                  s.view_quality.legacy_driver_restriction},
                                 0, sink);
        return {SessionSceneFrameKind::normal, std::move(camera), screen_result.entry_count};
    }
    catch (...)
    {
        // The failed suffix may already have changed source state. A repeated
        // call must not replay it against the same live level.
        detach_world_polygon_level(s.polygon_frame);
        throw;
    }
}
} // namespace sgn::retail
