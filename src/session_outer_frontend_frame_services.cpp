#include "sgn/session_outer_frame_services.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_loading_presentation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/polygon_submission_capacity.hpp"

namespace sgn::retail
{
SessionOuterFrameServices::SessionOuterFrameServices(FrontendState &frontend, SessionWorldMaterials &materials,
                                                     PolygonSubmissionState &scratch)
    : frontend_(&frontend), source_materials_(&materials), materials_(materials), scratch_(scratch),
      expected_gamma_(nullptr)
{
    require_source_frontend(frontend); // ownership only; no assets, audio, device or source initialization
}
SessionOuterFrameServices::SessionOuterFrameServices(FrontendState &frontend, SessionWorldMaterials &materials,
                                                     PolygonSubmissionState &scratch, SelectedDeviceDepthBorrow depth)
    : SessionOuterFrameServices(frontend, materials, scratch)
{
    bind_source_depth(depth);
}
void SessionOuterFrameServices::bind_source_depth(SelectedDeviceDepthBorrow depth)
{
    if (!frontend_)
        throw FormatError("late selected-depth binding requires cold menu services");
    require_source_frontend(*frontend_);
    if (selected_depth_)
        throw FormatError("cold menu selected-depth borrow is already bound");
    selected_depth_.emplace(depth); // reference to actual selected descriptor+64, no copied default
}
SelectedDeviceDepthBorrow SessionOuterFrameServices::selected_depth() const
{
    if (!selected_depth_)
        throw FormatError("cold menu clear requires actual selected descriptor+64 binding");
    return *selected_depth_;
}
void SessionOuterFrameServices::detach_source_backend_for_shutdown()
{
    if (!frontend_)
        throw FormatError("source shutdown detach requires cold menu frame services");
    frontend_->shared_platform()->require_owner_thread();
    source_backend_ = nullptr;
    expected_gamma_ = nullptr;
    world_resources_.reset();
    audio_.reset();
    selected_depth_.reset();
    source_backend_detached_ = true;
    // No access to material registry/backend, no source state reset or unload.
}
void SessionOuterFrameServices::require_source_frontend(const FrontendState &frontend) const
{
    if (source_backend_detached_)
        throw FormatError("cold frame services were detached for shutdown");
    if (frontend_ != &frontend || !source_materials_)
        throw FormatError("cold menu frames belong to another frontend");
    frontend.shared_platform()->require_owner_thread();
    const auto owner = source_materials_->require_registry_live();
    if (!source_materials_->source_registry_only_ || owner != frontend.shared_platform() ||
        &owner->state() != &frontend.world())
        throw FormatError("cold menu frames require the same canonical source material/world owner");
}
RaceSessionState &SessionOuterFrameServices::world() const
{
    if (frontend_)
    {
        require_source_frontend(*frontend_);
        return frontend_->world();
    }
    if (!session_)
        throw FormatError("frame service has no world owner");
    session_->shared_platform()->require_owner_thread();
    return session_->state();
}
RaceSession &SessionOuterFrameServices::race_session() const
{
    (void)world();
    if (!session_)
        throw FormatError("race-only frame borrow reached from cold menu services");
    return *session_;
}
void SessionOuterFrameServices::bind_source_backend(MaterialTextureBackend &backend, LightGammaDevice &gamma)
{
    if (!frontend_)
        throw FormatError("source backend binding requires cold menu frame services");
    require_source_frontend(*frontend_);
    if (source_backend_ || expected_gamma_)
        throw FormatError("cold menu frame backend is already bound");
    if (source_materials_->failed_ || source_materials_->attempted_)
        throw FormatError("bind cold menu backend before the first source material load");
    // NEW HOST storage admission, before publishing this backend. Core's
    // retained scratch starts empty;431300 borrows a LoadingFrameWorld before
    // the first material load. Keep the same58D818 representation used by all
    // later UI calls. This is not a source allocation or readiness store.
    // The existing helper preserves retained words, never shrinks, and leaves
    // the owner unchanged if budget admission/allocation fails.
    prepare_polygon_submission_ui_capacity(scratch_, desktop_polygon_submission_budget);
    source_backend_ = &backend;
    expected_gamma_ = &gamma; // host identity only, no source writes
}
WorldMaterialLoadingState &SessionOuterFrameServices::source_loading_state(const MaterialTextureBackend &backend) const
{
    if (!frontend_)
        throw FormatError("source loading borrow requires cold menu frame services");
    require_source_frontend(*frontend_);
    if (source_backend_ != &backend || !expected_gamma_)
        throw FormatError("source loading frame backend is missing or foreign");
    if (source_materials_->failed_ ||
        (source_materials_->loaded_backend_ && source_materials_->loaded_backend_ != &backend))
        throw FormatError("source loading frame has a failed or foreign material load");
    if (source_renderer_reset_epoch_)
    {
        if (source_material_loading_epoch_ || *source_renderer_reset_epoch_ != source_materials_->source_epoch_ ||
            source_materials_->source_epoch_phase_ != SourceMaterialEpochPhase::renderer_reserved ||
            !source_materials_->state_.names.materials.empty())
            throw FormatError("431300 borrow requires its reserved current epoch and reset physical table");
    }
    else if (source_material_loading_epoch_)
    {
        if (*source_material_loading_epoch_ != source_materials_->source_epoch_ ||
            source_materials_->source_epoch_phase_ != SourceMaterialEpochPhase::materials_reserved)
            throw FormatError("42C510 loading borrow requires its reserved current material epoch");
    }
    else if (source_next_scene_)
    {
        (void)source_materials_->next_scene_material_state_for(
            *source_next_scene_, frontend_->shared_platform()->source_scene_resources(), backend);
    }
    else
        source_materials_->require_source_cold_resource_epoch();
    return source_materials_->state_;
}
std::uint32_t &SessionOuterFrameServices::source_loading_shown_word(WorldMaterialLoadingState &state,
                                                                    const MaterialTextureBackend &backend) const
{
    if (&source_loading_state(backend) != &state)
        throw FormatError("445A40 requires the same material owner as the cold menu frame binding");
    return source_materials_->source_loading_shown_word();
}
std::uint32_t &SessionOuterFrameServices::source_loading_tile_base(WorldMaterialLoadingState &state,
                                                                   const MaterialTextureBackend &backend) const
{
    (void)source_loading_shown_word(state, backend);
    return world().ui.loading_tile_base;
}
PhysicalMaterialStorage &SessionOuterFrameServices::source_capture_storage(const MaterialTextureBackend &backend) const
{
    return source_loading_state(backend).names.materials;
}
PhysicalMaterialStorage &SessionOuterFrameServices::source_menu_replacement_storage(
    const MaterialTextureBackend &backend) const
{
    (void)source_loading_state(backend);
    return source_next_scene_ ? source_materials_->next_scene_replacement_storage(*source_next_scene_, backend)
                              : source_materials_->source_frontend_replacement_storage(backend);
}
const WorldMaterialLoadingState &SessionOuterFrameServices::drawing_materials() const
{
    if (!frontend_)
        return materials_.validate_for(race_session());
    if (!source_backend_)
        throw FormatError("cold menu draw reached before actual renderer backend binding");
    (void)source_menu_replacement_storage(*source_backend_); // actual fonts and42E772 publication
    const auto &scene = frontend_->shared_platform()->source_scene_resources();
    return source_next_scene_ ? materials_.next_scene_material_state_for(*source_next_scene_, scene, *source_backend_)
                              : materials_.source_material_state_for(scene, *source_backend_);
}
LoadingFrameWorld SessionOuterFrameServices::borrow_source_loading_frame(WorldMaterialLoadingState &state,
                                                                         const MaterialTextureBackend &backend)
{
    (void)source_loading_shown_word(state, backend);
    return loading_frame(state); // fresh span after each real42C480/prelude
}
LoadingPresentationWorld SessionOuterFrameServices::borrow_source_loading_presentation(
    WorldMaterialLoadingState &state, const MaterialTextureBackend &backend)
{
    auto frame = borrow_source_loading_frame(state, backend);
    auto &s = world();
    return {{{s.tick_clock, s.simulation_tick, s.source_scheduler.fixed_mode_a, s.source_scheduler.fixed_mode_b,
              s.controls, s.overlays, s.startup.fields_62d480, s.startup.fields_62dcf0},
             frame,
             s.ui.loading_fade,
             s.light_fade,
             expected_gamma_},
            s.ui.loading_tile_base};
}
} // namespace sgn::retail
