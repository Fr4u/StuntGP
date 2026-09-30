#include "sgn/session_outer_frame_services.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_loading_presentation.hpp"

namespace sgn::retail
{
RaceSession &SessionOuterFrameServices::checked_session(RaceSession &session, const RaceOverlayTextures &textures)
{
    session.shared_platform()->require_owner_thread();
    textures.require_for(session.shared_platform());
    return session;
}
SessionOuterFrameServices::SessionOuterFrameServices(RaceSession &session, const SessionWorldMaterials &materials,
                                                     PolygonSubmissionState &scratch, RaceOverlayTextures &textures,
                                                     SelectedDeviceDepthBorrow selected_depth,
                                                     std::optional<SessionOuterWorldResourceBorrow> resources,
                                                     LightGammaDevice *expected_gamma)
    : session_(&checked_session(session, textures)), materials_(materials), scratch_(scratch), textures_(&textures),
      selected_depth_(selected_depth), world_resources_(resources), expected_gamma_(expected_gamma),
      audio_(session_->audio_view())
{
}

SessionWorldFrameResources SessionOuterFrameServices::borrow_world_resources()
{
    (void)race_session();
    (void)world();
    if (!world_resources_)
        throw FormatError("Session outer world resource producer is missing");
    const auto &binding = *world_resources_;
    if (&binding.session != &race_session())
        throw FormatError("Session outer world resources belong to a different Session");
    // Copy only the current view at this getter, never at construction. The
    // actual source children retain ownership of their access-order checks.
    const auto current = binding.current;
    switch (binding.gamma_binding)
    {
    case SessionOuterGammaBinding::source_device_absent:
        if (current.gamma)
            throw FormatError("Session outer absent gamma binding has a receiver");
        if (expected_gamma_)
            throw FormatError("Session outer gamma absence differs from its capture backend");
        break;
    case SessionOuterGammaBinding::source_device_present:
        if (!current.gamma)
            throw FormatError("Session outer gamma receiver is missing");
        if (current.gamma != expected_gamma_)
            throw FormatError("Session outer gamma receiver differs from its capture backend");
        break;
    case SessionOuterGammaBinding::unresolved:
    default:
        throw FormatError("Session outer gamma device availability is unresolved");
    }
    // Empty animation/adjacent spans and a null color audit are NOT success
    // substitutes. Existing45E4F0 rejects a reached unresolved descriptor,
    // adjacent sample or source-unwritten color. Do not scan particles here:
    // that would move those checks ahead of402E90/418180/442500.
    // Budgets/audits are explicitly selected host inputs, copied unchanged.
    return current;
}

TextSubmission SessionOuterFrameServices::borrow_text()
{
    (void)world();
    const auto &bank = drawing_materials();
    if (frontend_)
    {
        if (source_next_scene_)
            (void)materials_.next_scene_fonts(*source_next_scene_, *source_backend_);
        else
            (void)materials_.source_frontend_fonts(*source_backend_);
    }
    else
        (void)materials_.validate_fonts_for(race_session());
    auto &s = world();
    return {s.ui.fonts,
            s.ui.selected_material,
            scratch_.specular_enabled,
            bank.names.materials.active_materials(),
            s.view_quality.second,
            s.view_quality.legacy_driver_restriction,
            s.viewport};
}
std::span<PolygonSubmissionVertex, 4> SessionOuterFrameServices::borrow_vertices()
{
    (void)world();
    if (scratch_.alpha_vertices.size() < 4)
        throw FormatError("Session outer UI requires prepared shared four-vertex scratch");
    return std::span<PolygonSubmissionVertex, 4>{scratch_.alpha_vertices.data(), 4};
}
RaceHudSubmission SessionOuterFrameServices::borrow_countdown()
{
    (void)race_session();
    auto rendering = borrow_text();
    auto vertices = borrow_vertices();
    auto &s = world();
    return {{s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
            s.vehicles,
            s.clock,
            s.records,
            s.ranking,
            s.cars,
            s.game_text,
            s.split_setting,
            s.frame_pause,
            *textures_,
            s.ui.stars,
            s.ui.descriptor,
            vertices,
            rendering,
            *audio_};
}
PauseAgeFadeWorld SessionOuterFrameServices::borrow_age_fade()
{
    (void)world();
    const auto &bank = drawing_materials();
    auto vertices = borrow_vertices();
    auto &s = world();
    return {s.ui.descriptor, vertices,   s.ui.selected_material, bank.names.materials.active_materials(), s.viewport,
            s.frame_pause,   s.pause.age};
}
LoadingFrameWorld SessionOuterFrameServices::borrow_loading_frame()
{
    return loading_frame(drawing_materials());
}
LoadingFrameWorld SessionOuterFrameServices::loading_frame(const WorldMaterialLoadingState &bank)
{
    auto vertices = borrow_vertices();
    auto &s = world();
    const auto words = device_words();
    return {{s.ui.descriptor, vertices, s.ui.selected_material, bank.names.materials.active_materials(), s.viewport,
             s.controls, s.overlays, s.ui.loading_tile_base, s.ui.loading_fade, s.frame_pause, s.ui.second_pause},
            scratch_,
            selected_depth().word_64,
            s.outer.device.fog_a,
            s.outer.device.fog_b,
            words.scene_active,
            words.finalized,
            words.render_counter};
}
DemoTailWords SessionOuterFrameServices::borrow_demo_words()
{
    (void)world();
    // 40E030 borrows these before its HUD gate. Never require fonts/materials,
    // alpha capacity, a gamma receiver, or effect data just to bind these words.
    return session_outer_demo_words(world());
}
SessionOuterDeviceWords SessionOuterFrameServices::device_words()
{
    (void)world();
    return session_outer_device_words(world());
}
SessionPolygonCapabilities SessionOuterFrameServices::capabilities()
{
    (void)world();
    return session_outer_polygon_capabilities(world(), selected_depth());
}
} // namespace sgn::retail
