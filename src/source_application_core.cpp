#include "sgn/source_application_core.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/source_application_final_shutdown.hpp"
#include "sgn/source_application_menu_composition.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/source_application_surfaces_late_bound.hpp"
#include "sgn/new_host_d3d12_application_surfaces.hpp"
#include "sgn/source_frontend_material_binding.hpp"
#include "sgn/source_menu_music.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include "sgn/source_race_level_script.hpp"
#include "sgn/source_world_submission.hpp"
#include "sgn/source_race_ring_render.hpp"
#include "sgn/source_race_hud_draw.hpp"
#include "sgn/source_camera_startup.hpp"
#include "sgn/source_cold_scene_tail.hpp"
#include "sgn/source_sprite_scene.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/retail_world_view_prefix.hpp"
#include "sgn/source_renderer_reset.hpp"
#include "sgn/source_renderer_bootstrap.hpp"
#include "sgn/source_numeric_win64.hpp"
#include "sgn/retail_level_selection.hpp"
#include "sgn/retail_viewport.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/retail_loading_presentation.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_host_counter.hpp"
#include "sgn/source_host_pacing.hpp"
#include <stdexcept>
#include <limits>
#include <type_traits>
#include <vector>

namespace sgn::retail
{
namespace
{
// NEW synchronous wiring of reviewed source children. These adapters own no
// source world or copied ring. Missing stack words still fail at their real read.
class CoreRaceViewChildren final : public SourceRaceViewStageRequired
{
    SourceVehicleDispatchUnavailable remaining_;
    SourceVehicleDrawStackUnavailable stack_;
    SourceVehicleDrawKnownPrefix stack_prefix_{stack_};
    SourceVehicleRingBinding rings_;
    SourceVehicleDrawPresentation presentation_{remaining_, stack_prefix_, rings_};
    SourceVehicleDrawGraph graph_{presentation_, stack_prefix_};

  public:
    void vehicles_418180(RaceSessionPlatform &platform) override
    {
        SGN_PROFILE_SCOPE("entities.vehicles_418180");
        append_source_vehicle_draw_418180(platform, graph_);
    }
    void impact_trail_41f2f0(RaceSessionPlatform &platform, const contact::effects::ImpactParticle &particle,
                             std::uint32_t profile, std::uint32_t cursor, std::uint32_t count, float scale,
                             float alpha) override
    {
        SGN_PROFILE_SCOPE("entities.impact_ring_41f2f0");
        append_source_impact_ring_41f2f0(platform, particle, profile, cursor, count, scale, alpha);
    }
};
// 433F40 resolves current materials at its reached site. HUD/countdown borrow
// the same frame owners. Pause/results/demo remain the caller's requirement.
template <class Submit> class CoreDrawChildren final : public SourceCoreRaceDrawRequired
{
    SourceCoreRaceDrawRequired &required_;
    Submit &submit_;
    CoreRaceViewChildren view_;

  public:
    CoreDrawChildren(SourceCoreRaceDrawRequired &required, Submit &submit) : required_(required), submit_(submit) {}
    void perform(SourceCoreRaceDrawChild child, std::uint32_t site, SourceCoreRaceDrawContext &context) override
    {
        if (child == SourceCoreRaceDrawChild::world_submission)
        {
            submit_();
            return;
        }
        if (child == SourceCoreRaceDrawChild::entities)
        {
            prepare_source_race_view_stage_442500(*context.frontend.shared_platform(), view_);
            return;
        }
        if (child == SourceCoreRaceDrawChild::hud)
        {
            submit_source_race_hud_44ae40(context);
            return;
        }
        if (child == SourceCoreRaceDrawChild::countdown)
        {
            submit_source_race_countdown_449b50(context);
            return;
        }
        required_.perform(child, site, context);
    }
};
// NEW repair (PORT-FIX proposal P1): the Core's single source counter is the
// rebased 6000 Hz host counter, so 434820's low DWORD (434830) does not wrap
// in a session. 434840/442100/442490 and their arithmetic are unchanged.
using ApplicationCounter = SourceHostRebasedCounter;
std::shared_ptr<RaceSessionPlatform> make_platform(SourceApplicationLiteralData &app, const std::filesystem::path &root)
{
    auto platform = std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{}, root);
    platform->bind_source_application_literals(app);
    platform->prepare_host_assets();
    platform->prepare_source_scene_resources();
    return platform;
}
} // namespace
struct SourceApplicationCore::Impl
{
    SourceApplicationLiteralData &application;
    NewHostNativeWindowBorrow native;
    std::unique_ptr<Renderer> &renderer_slot; // borrow only, App remains sole owner
    std::shared_ptr<RaceSessionPlatform> platform;
    FrontendState frontend;
    SessionWorldMaterials materials;
    PolygonSubmissionState submission;
    SourceDrawStartupFailure &draw_failure; // same retained device-start receiver
    LightGammaD3D12 gamma;
    LightGammaDevice *source_gamma_5da1dc{}; // PE null, published only by432070
    // Sole retained BSS words. WNDPROC, startup and entry borrow these slots.
    std::uint32_t active_61c374{}, mode_61c364{};
    std::uint8_t character_62ddfc{};
    SourceApplicationWindowClock window_clock;
    ApplicationCounter counter;
    // NEW HOST pacing (R18 P2) for the 445940 loading loop only: its 4458F0
    // clock read waits until the unchanged 442490 test has a due tick, so a
    // loading frame (4458F0+4458B0 Present) runs at the 60 Hz source cadence.
    SourceHostLoadingPacedCounter loading_counter{counter, platform->state().tick_clock,
                                                  platform->state().source_scheduler.fixed_mode_a,
                                                  platform->state().source_scheduler.fixed_mode_b};
    Win32SourceColdTimePlatform time;
    NativeSourceBootFilePlatform files;
    SessionOuterFrameServices frames;
    SourceMenuPresentationBackend presentation;
    SourceLateBoundApplicationSurfaces bridge;
    NewHostD3D12ApplicationSurfaces lifecycle;
    NewHostD3D12WindowStartup startup;
    SourceD3D12DeviceProduction production;
    RetailLoadingPresentation loading;
    SourceMenuMusic music;
    SourceMenuSampleNative samples;
    std::unique_ptr<SourceFrontendMaterialBinding> menu_materials;
    // Retired adapters stay alive until Core destruction. They retain immutable
    // old epochs and never regain access. Menu is destroyed before this list.
    std::vector<std::unique_ptr<SourceFrontendMaterialBinding>> retired_menu_materials;
    std::unique_ptr<SourceMainMenuChildren> menu;
    SourceSavePageServices *saved_save{};
    SourceMainMenuRequiredServices *saved_required{};
    NativeSourceMenuCrt *saved_crt{};
    SourceApplicationShutdownServices *saved_shutdown{};
    bool menu_lifetime_closed{}; // Terminal NEW HOST state, never source60D61C.
    SourceSceneEpochTransition resource_transition{SourceSceneEpochTransition::cold};
    std::uint64_t released_backend_epoch{};         // actual431810 return, not a texture count heuristic
    std::uint64_t prepared_backend_epoch{};         // actual completed42E6FA renderer prefix; host observation only
    std::uint64_t material_backend_epoch{};         // actual42C510/44B1A0 return; no font/count readiness
    std::uint64_t font_backend_epoch{};             // actual428FA0/42E772 completion; no tail/draw readiness
    SessionOuterFrameCadence source_update_cadence; // NEW host entry guard, not a second source clock
    SessionOuterFrameCadence source_draw_cadence;   // independent accepted draw serial
    bool source_race_invocation_active{};           // reject cross-entry draw/update recursion
    const std::size_t *app_callback_depth{};
    const bool *app_callbacks_retired{}; // same App lvalues retained by Composition

    SourceWindowMessageWords words()
    {
        return {active_61c374, mode_61c364, character_62ddfc};
    }
    Impl(SourceApplicationLiteralData &app, const std::filesystem::path &root, NewHostNativeWindowBorrow window,
         std::unique_ptr<Renderer> &renderer, const std::exception_ptr &failure, RenderOptions options,
         PolygonSubmissionCaptureLimits limits, SourceDrawStartupFailure &startup_failure,
         std::optional<std::string> tz)
        : application(app), native(window), renderer_slot(renderer), platform(make_platform(app, root)),
          frontend(std::shared_ptr<RaceSessionState>(platform, &platform->state()), platform),
          materials(SourceColdMaterialAttachmentOnly{}, platform), draw_failure(startup_failure), time(std::move(tz)),
          files(root, platform->state().boot_config), frames(frontend, materials, submission),
          presentation(frontend, frames), bridge(frontend),
          lifecycle(frontend, words(), mode_61c364, native, renderer, presentation, gamma, source_gamma_5da1dc,
                    failure),
          startup(frontend, native, words(), bridge, failure),
          production(frontend, materials, frames, presentation, app, submission, native.window, renderer, options,
                     limits, gamma, source_gamma_5da1dc, lifecycle, startup, startup_failure),
          loading(frames, loading_counter, presentation.sink(), presentation.device()), music(platform),
          samples(platform)
    {
        if (native.window || renderer)
            throw FormatError("Application core must precede HWND and GPU creation");
        bridge.attach_host(lifecycle);
    }
};
SourceApplicationCore::SourceApplicationCore(SourceApplicationLiteralData &app, std::filesystem::path root,
                                             NewHostNativeWindowBorrow native, std::unique_ptr<Renderer> &renderer,
                                             const std::exception_ptr &failure, RenderOptions options,
                                             PolygonSubmissionCaptureLimits limits,
                                             SourceDrawStartupFailure &startup_failure, std::optional<std::string> tz)
    : impl_(
          std::make_unique<Impl>(app, root, native, renderer, failure, options, limits, startup_failure, std::move(tz)))
{
}
SourceApplicationCore::~SourceApplicationCore() = default;
const std::shared_ptr<RaceSessionPlatform> &SourceApplicationCore::platform() const
{
    return impl_->platform;
}
FrontendState &SourceApplicationCore::frontend()
{
    return impl_->frontend;
}
SourceMenuSampleNative &SourceApplicationCore::source_samples()
{
    return impl_->samples;
}
SourceFrontendMaterialBinding &SourceApplicationCore::source_race_materials()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    if (s.menu_lifetime_closed || !s.menu_materials)
        throw FormatError("source race draw child requires this Core's current material adapter");
    return *s.menu_materials;
}
SourceWindowMessageWords SourceApplicationCore::window_words()
{
    return impl_->words();
}
SourceLateBoundApplicationSurfaces &SourceApplicationCore::surface_bridge()
{
    return impl_->bridge;
}
NewHostD3D12WindowStartup &SourceApplicationCore::window_startup()
{
    return impl_->startup;
}
SourceD3D12DeviceProduction &SourceApplicationCore::device_production()
{
    return impl_->production;
}
LightGammaDevice *const &SourceApplicationCore::source_gamma_publication() const &
{
    impl_->platform->require_owner_thread();
    return impl_->source_gamma_5da1dc;
}
SessionWorldMaterials &SourceApplicationCore::materials()
{
    return impl_->materials;
}
SourceSceneEpochTransition SourceApplicationCore::source_scene_epoch_transition() const
{
    impl_->platform->require_owner_thread();
    return impl_->resource_transition;
}
void SourceApplicationCore::reject_resource_epoch_reentry()
{
    auto &s = *impl_;
    if (s.resource_transition == SourceSceneEpochTransition::releasing_images ||
        s.resource_transition == SourceSceneEpochTransition::releasing_allocations ||
        s.resource_transition == SourceSceneEpochTransition::beginning_allocations ||
        s.resource_transition == SourceSceneEpochTransition::resetting ||
        s.resource_transition == SourceSceneEpochTransition::loading_script ||
        s.resource_transition == SourceSceneEpochTransition::preparing_models ||
        s.resource_transition == SourceSceneEpochTransition::preparing_renderer ||
        s.resource_transition == SourceSceneEpochTransition::loading_materials ||
        s.resource_transition == SourceSceneEpochTransition::initializing_fonts ||
        s.resource_transition == SourceSceneEpochTransition::preparing_scene_storage ||
        s.resource_transition == SourceSceneEpochTransition::initializing_camera ||
        s.resource_transition == SourceSceneEpochTransition::finishing_scene_tail ||
        s.resource_transition == SourceSceneEpochTransition::rebinding_adapters)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw FormatError("reentrant source resource transition permanently invalidated this epoch");
    }
}
void SourceApplicationCore::release_source_menu_images_431810()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    const bool cold = s.resource_transition == SourceSceneEpochTransition::cold &&
                      s.materials.source_epoch_phase_ == SourceMaterialEpochPhase::cold;
    const bool current = s.resource_transition == SourceSceneEpochTransition::adapters_ready &&
                         s.materials.source_epoch_phase_ == SourceMaterialEpochPhase::adapters_ready;
    if (s.menu_lifetime_closed || (!cold && !current) || !s.materials.source_scene_tail_completed_)
        throw FormatError("431810 requires this Core's completed current source scene and unused unload reservation");
    s.resource_transition = SourceSceneEpochTransition::releasing_images;
    try
    {
        // The retained presentation validates the cold binding or its actual
        // committed next-scene identity before releasing the live GPU bank.
        s.presentation.release_cold_menu_images_431810();
        auto reset = s.production.reset_context(s.frontend);
        s.released_backend_epoch = reset.materials.resource_epoch();
        if (s.resource_transition != SourceSceneEpochTransition::releasing_images)
            throw FormatError("nested resource transition failure cannot publish completion");
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::invalidated;
        s.resource_transition = SourceSceneEpochTransition::images_released;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
}
void SourceApplicationCore::release_source_scene_allocations_42eaa0()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::images_released)
        throw FormatError("42EAA0 requires this Core's immediately preceding completed431810");
    s.resource_transition = SourceSceneEpochTransition::releasing_allocations;
    try
    {
        // NEW host lease retirement before the actual source free. Tickets
        // from the departing scene must not validate while its storage dies.
        // No source pointer, bank flag or GPU recipe is changed by detaching.
        detach_world_polygon_level(s.frontend.world().polygon_frame);
        s.platform->release_scene_allocations_42eaa0();
        if (s.resource_transition != SourceSceneEpochTransition::releasing_allocations)
            throw FormatError("nested resource transition failure cannot publish completion");
        s.resource_transition = SourceSceneEpochTransition::allocations_released;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
}
void SourceApplicationCore::begin_next_source_scene_allocations_42ea80()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::allocations_released ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::invalidated)
        throw FormatError("44204E requires this Core's completed ordered source unload");
    const auto &bank = s.platform->source_allocations();
    if (!bank.initialized() || bank.source_words().active != 0)
        throw FormatError("44204E requires the unchanged bank after this Core's42EAA0");
    s.resource_transition = SourceSceneEpochTransition::beginning_allocations;
    try
    {
        s.platform->begin_scene_allocations_42ea80(); // 44204E, existing full42EA00 then three stores
        if (s.resource_transition != SourceSceneEpochTransition::beginning_allocations)
            throw FormatError("nested resource transition failure cannot publish bank-begin completion");
        s.resource_transition = SourceSceneEpochTransition::allocations_begun;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
}
void SourceApplicationCore::reset_next_source_scene_epoch_42e510()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::allocations_begun)
        throw FormatError("next resource epoch requires this Core's completed44204E ->42EA80");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended())
        throw FormatError("next resource epoch requires live idle App/presentation owners outside native dispatch");
    auto reset = s.production.reset_context(s.frontend);
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_entries() != 0 ||
        reset.materials.resource_epoch() != s.released_backend_epoch)
        throw FormatError("next resource epoch has different or reused GPU/material owners");
    (void)reset.materials.bindings_snapshot(); // live healthy backend; keeps no additional owner
    // Retain the same menu/material/font/renderer objects. Stable material
    // adapters reject the new epoch; raw synchronous borrows must have ended.
    // Source front/back recipes and their own in-flight GPU references survive.
    s.resource_transition = SourceSceneEpochTransition::resetting;
    try
    {
        s.materials.reset_source_next_scene_epoch_42e510();
        if (s.resource_transition != SourceSceneEpochTransition::resetting)
            throw FormatError("nested resource transition failure cannot publish completion");
        s.resource_transition = SourceSceneEpochTransition::reset_completed;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
}
void SourceApplicationCore::load_next_source_level_script_42e6e6()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::reset_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::reset_completed)
        throw FormatError("42E6E6 requires this Core's completed next-epoch42E510");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E6E6 requires the same source world and active44204E allocation scope");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended())
        throw FormatError("42E6E6 requires live idle App/presentation owners outside native dispatch");
    auto reset = s.production.reset_context(s.frontend); // pure identity borrow; no renderer reset
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_entries() != 0 ||
        reset.materials.resource_epoch() != s.released_backend_epoch)
        throw FormatError("42E6E6 has different or reused GPU/material owners");
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::loading_script;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::script_reserved;
    try
    {
        // Source4452E0 and4453A0 read actual612BEC at their original sites.
        // This receiver closes both the exact1 race branch and other targets;
        // do not route scene2 through the intentionally cold-only adapter.
        load_source_race_level_script_42d070(*s.platform, s.files); // 42E6E6
        if (s.resource_transition != SourceSceneEpochTransition::loading_script ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::script_reserved)
            throw FormatError("nested resource transition failure cannot publish script completion");
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::script_completed;
        s.resource_transition = SourceSceneEpochTransition::script_completed;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // 42D15E released its real script;42D168 returned. Next42E6EB ->44FD60
    // has NOT executed. Old resource/font borrows remain invalidated.
}
void SourceApplicationCore::prepare_next_source_scene_models_42e6eb()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::script_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::script_completed)
        throw FormatError("42E6EB requires this Core's completed next-epoch42D070");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E6EB requires the same source world and active44204E allocation scope");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended())
        throw FormatError("42E6EB requires live idle App/presentation owners outside native dispatch");
    auto reset = s.production.reset_context(s.frontend); // pure identity borrow; no renderer reset
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_entries() != 0 ||
        reset.materials.resource_epoch() != s.released_backend_epoch)
        throw FormatError("42E6EB has different or reused GPU/material owners");
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::preparing_models;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::models_reserved;
    try
    {
        reset_view_matrix_cursor(s.platform->state().view_matrices); // 42E6EB ->44FD60
        // Source4342C0 includes all six allocations, immediate publications,
        //  both434030 calls and44EE10 on the actual5DA1E0 matrix owner.
        initialize_source_sprite_scene_4342c0(s.platform); // 42E6F0
        if (s.resource_transition != SourceSceneEpochTransition::preparing_models ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::models_reserved)
            throw FormatError("nested resource transition failure cannot continue to42F090");
        // Read actual612BE8 only here, after4342C0. The fixed-extent spans
        //  borrow the same six selection/car records, never a compacted copy.
        auto &world = s.platform->state();
        s.platform->source_scene_resources().load_source_requests_42f090(
            s.platform->source_allocations(), world.scene_mode, world.selection, world.cars); // 42E6F5
        if (s.resource_transition != SourceSceneEpochTransition::preparing_models ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::models_reserved)
            throw FormatError("nested resource transition failure cannot publish model-prefix completion");
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::models_completed;
        s.resource_transition = SourceSceneEpochTransition::models_completed;
    }
    catch (...)
    {
        // No rollback, early release, request replay, synthetic ready word or
        //  replacement registry. Children retain their reached partial writes.
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // 42F15E returned. Next42E6FA MOV EAX,[6244F8] has NOT executed.
    // All existing material/font epoch borrows remain invalidated.
}
void SourceApplicationCore::prepare_next_source_scene_renderer_42e6fa()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::models_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::models_completed)
        throw FormatError("42E6FA requires this Core's completed next-epoch42F090");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E6FA requires the same source world and active44204E allocation scope");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_)
        throw FormatError("42E6FA requires live idle App/presentation owners outside native dispatch");
    auto reset = s.production.reset_context(s.frontend); // host identity borrow; no source call
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_entries() != 0 ||
        reset.materials.resource_epoch() != s.released_backend_epoch || !s.materials.state_.names.materials.empty())
        throw FormatError("42E6FA has different GPU/material owners or an unreleased resource table");
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::preparing_renderer;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::renderer_reserved;
    try
    {
        auto &world = s.platform->state();
        const auto initialized = world.outer.device.initialized_6244f8;    // 42E6FA, capture exactly once
        s.frames.source_renderer_reset_epoch_ = s.materials.source_epoch_; // host-only synchronous scope
        if (initialized == 0)
        {
            initialize_legacy_driver_restriction(world.view_quality); // 42E708 ->42E6D0
            // Existing bootstrap keeps all reached source stores. The current
            //  retained production receiver rejects42F6A0 over live surfaces;
            //  do not replace it, guess success or force6244F8 back to one.
            initialize_source_renderer_431360(
                {s.frontend, s.materials, s.application, s.submission, s.production}); // 42E70D
            world.outer.device.initialized_6244f8 = 1; // 42E712 only after successful431360
            world.view_quality.first = 1;              // 42E718
            world.view_quality.second = 1;             // 42E71E
        }
        else
        {
            reset_source_renderer_431300(reset); // 42E726, includes all three clear/Flip pairs
        }
        s.frames.source_renderer_reset_epoch_.reset(); // close host permission, no source rollback
        if (s.resource_transition != SourceSceneEpochTransition::preparing_renderer ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::renderer_reserved)
            throw FormatError("nested resource transition failure cannot continue to42E72B");
        prepare_source_numeric_win64_429510();                 // 42E72B, actual current-thread x87/SSE transport
        s.materials.initialize_source_material_roots_42e3e0(); // 42E730, reads actual477C30
        update_viewport_ui_scale(world.viewport);              // 42E735 ->425070
        randomize_vehicle_liveries(world.vehicle_texture_selection, world.random); // 413190 first413110
        remap_level_vehicle_textures(
            s.platform->source_scene_resources().models(), world.cars,
            world.vehicle_texture_selection.random_liveries); // 42E73A/73F: all liveries, then all tyres
        if (s.resource_transition != SourceSceneEpochTransition::preparing_renderer ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::renderer_reserved)
            throw FormatError("nested resource transition failure cannot publish renderer-prefix completion");
        s.prepared_backend_epoch = reset.materials.resource_epoch(); // observes431790's actual new bank
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::renderer_prefix_completed;
        s.resource_transition = SourceSceneEpochTransition::renderer_prefix_completed;
    }
    catch (...)
    {
        s.frames.source_renderer_reset_epoch_.reset(); // close host permission even after a partial frame
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // 42E744 ->42C510 has NOT executed. Its next-epoch material/font loading
    //  contract remains separate; cold claims/guards and source words survive.
}
void SourceApplicationCore::load_next_source_scene_materials_42e744()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::renderer_prefix_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::renderer_prefix_completed)
        throw FormatError("42E744 requires this Core's completed next-epoch renderer/name prefix");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E744 requires the same source world and active44204E allocation scope");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_)
        throw FormatError("42E744 requires live idle App/presentation owners outside native dispatch");
    auto reset = s.production.reset_context(s.frontend); // pure borrow, no reset or upload
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_entries() != 0 ||
        reset.materials.resource_epoch() != s.prepared_backend_epoch || !s.materials.state_.names.materials.empty())
        throw FormatError("42E744 has different backend/table owners or drifted renderer-prefix resources");
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::loading_materials;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::materials_reserved;
    try
    {
        s.frames.source_material_loading_epoch_ = s.materials.source_epoch_;
        s.materials.load_source_next_scene_materials_42e744(s.files, reset.materials, s.loading); // 42E744
        s.frames.source_material_loading_epoch_.reset(); // close synchronous loading permission
        if (s.resource_transition != SourceSceneEpochTransition::loading_materials ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::materials_reserved)
            throw FormatError("nested resource transition failure cannot continue to42E749");
        release_platform_source_archive_42e749(*s.platform); // actual44B1A0, only after42C510 returned
        s.material_backend_epoch = reset.materials.resource_epoch();
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::materials_completed;
        s.resource_transition = SourceSceneEpochTransition::materials_completed;
    }
    catch (...)
    {
        s.frames.source_material_loading_epoch_.reset(); // host permission only; no source/GPU cleanup
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // Stop BEFORE42E74E ->428FA0. No new font resources,42E772 count or
    //  scene-tail completion is implied, and Composition still rejects warm load.
}
void SourceApplicationCore::initialize_next_source_scene_fonts_42e74e()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::materials_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::materials_completed)
        throw FormatError("42E74E requires this Core's completed current42C510/44B1A0 stage");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E74E requires the same world and actual44204E allocation scope");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_)
        throw FormatError("42E74E requires live idle App/presentation owners outside resource/loading callbacks");
    auto reset = s.production.reset_context(s.frontend); // same produced backend, no source reset
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_epoch() != s.material_backend_epoch)
        throw FormatError("42E74E backend differs from or changed since this epoch's42C510 completion");
    reset.materials.require_font_upload_ready();
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::initializing_fonts;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::fonts_reserved;
    try
    {
        s.materials.initialize_source_next_scene_fonts_42e74e(s.files, reset.materials);
        if (s.resource_transition != SourceSceneEpochTransition::initializing_fonts ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::fonts_reserved)
            throw FormatError("nested resource failure cannot publish next428FA0/42E772 completion");
        s.font_backend_epoch = reset.materials.resource_epoch();
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::fonts_count_completed;
        s.resource_transition = SourceSceneEpochTransition::fonts_count_completed;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // 42E778 ->42E430 has NOT executed. All ordinary material/font/draw
    // borrowers remain invalid and Composition's warm guard is unchanged.
}
void SourceApplicationCore::prepare_next_source_scene_storage_42e778()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::fonts_count_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::fonts_count_completed)
        throw FormatError("42E778 requires this Core's completed current428FA0/42E772 stage");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E778 requires the same world and active44204E allocation scope");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_)
        throw FormatError("42E778 requires live idle App/presentation owners outside resource/loading callbacks");
    auto reset = s.production.reset_context(s.frontend); // identity borrow only, no reset or upload
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_epoch() != s.font_backend_epoch)
        throw FormatError("42E778 backend differs from or changed since this epoch's428FA0/42E772 completion");
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::preparing_scene_storage;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::scene_storage_reserved;
    try
    {
        s.materials.prepare_source_next_scene_storage_42e778();
        if (s.resource_transition != SourceSceneEpochTransition::preparing_scene_storage ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::scene_storage_reserved ||
            reset.materials.resource_epoch() != s.font_backend_epoch)
            throw FormatError("nested resource failure cannot publish next42E430/427270/42E380 completion");
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::scene_storage_completed;
        s.resource_transition = SourceSceneEpochTransition::scene_storage_completed;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // Stop BEFORE42E787 ->402420, including when the current scene is2.
    // No cold-tail flag is cleared/set, no old borrower or warm loader is enabled.
}
void SourceApplicationCore::initialize_next_source_camera_42e787()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::scene_storage_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::scene_storage_completed)
        throw FormatError("42E787 requires this Core's completed current42E430/427270/42E380 stage");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E787 requires the same world and current active allocation bank");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_)
        throw FormatError("42E787 requires live idle App/presentation owners outside resource/loading callbacks");
    if (!s.saved_crt)
        throw FormatError("42E787 requires this Core's retained menu CRT receiver");
    s.saved_crt->require_platform(*s.platform);
    auto reset = s.production.reset_context(s.frontend); // identity borrow only
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_epoch() != s.font_backend_epoch)
        throw FormatError("42E787 backend changed since this epoch's actual font/count completion");
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::initializing_camera;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::camera_reserved;
    try
    {
        initialize_source_camera_402420(s.platform, s.files, *s.saved_crt); // 42E787
        // 42E78C calls44EE00, whose entire source body is RET.
        if (s.resource_transition != SourceSceneEpochTransition::initializing_camera ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::camera_reserved ||
            reset.materials.resource_epoch() != s.font_backend_epoch)
            throw FormatError("nested resource failure cannot publish next402420 completion");
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::camera_completed;
        s.resource_transition = SourceSceneEpochTransition::camera_completed;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // Stop before42E791/42B740. No full-tail, draw or Composition readiness.
}
void SourceApplicationCore::finish_next_source_scene_tail_42e791()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || s.resource_transition != SourceSceneEpochTransition::camera_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::camera_completed)
        throw FormatError("42E791 requires this Core's completed current402420 stage");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("42E791 requires the same world and current active allocation bank");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_)
        throw FormatError("42E791 requires live idle App/presentation owners outside resource callbacks");
    auto reset = s.production.reset_context(s.frontend); // same backend, no source reset
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_epoch() != s.font_backend_epoch)
        throw FormatError("42E791 backend changed since this epoch's font/count completion");
    (void)reset.materials.bindings_snapshot();
    s.resource_transition = SourceSceneEpochTransition::finishing_scene_tail;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::scene_tail_reserved;
    try
    {
        auto context = cold_scene_context(); // existing invocation type, no cold admission or new owners
        finish_source_scene_post_camera_42e791(context, s.source_gamma_5da1dc);
        if (s.resource_transition != SourceSceneEpochTransition::finishing_scene_tail ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::scene_tail_reserved ||
            reset.materials.resource_epoch() != s.font_backend_epoch)
            throw FormatError("nested resource failure cannot publish next scene-tail completion");
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::scene_tail_completed;
        s.resource_transition = SourceSceneEpochTransition::scene_tail_completed;
    }
    catch (...)
    {
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // Actual42E7F8 returned. Old-epoch adapters remain stale; Composition still
    // requires their explicit current-epoch rebind and the real race Core frame.
}
void SourceApplicationCore::rebind_next_source_scene_adapters()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    reject_resource_epoch_reentry();
    if (s.menu_lifetime_closed || !s.menu || !s.menu_materials ||
        s.resource_transition != SourceSceneEpochTransition::scene_tail_completed ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::scene_tail_completed)
        throw FormatError("adapter rebind requires this Core's completed next source scene tail and live menu");
    if (s.materials.require_registry_live() != s.platform || !s.platform->source_allocations().initialized() ||
        s.platform->source_allocations().source_words().active != 1)
        throw FormatError("adapter rebind requires this Core's same active allocation/world owners");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_)
        throw FormatError("adapter rebind requires idle live owners outside native/resource/loading callbacks");
    auto reset = s.production.reset_context(s.frontend); // identity borrow; no source reset
    if (&reset.frontend != &s.frontend || &reset.frames != &s.frames || &reset.presentation != &s.presentation ||
        !s.renderer_slot || !reset.materials.bound_to_renderer(*s.renderer_slot) ||
        s.materials.loaded_backend_ != &reset.materials || reset.materials.resource_epoch() != s.font_backend_epoch)
        throw FormatError("adapter rebind backend changed since this epoch's completed fonts and tail");
    s.frames.require_source_frontend(s.frontend);
    auto &language = static_cast<SourceMenuLanguageChildren &>(*s.menu);
    auto &consumption = static_cast<SourceMenuConsumptionChildren &>(*s.menu);
    if (s.frames.source_backend_ != &reset.materials || s.frames.expected_gamma_ != &s.gamma ||
        &consumption.frames_ != &s.frames || &consumption.sink_ != &s.presentation.sink() ||
        language.materials_ != s.menu_materials.get() || consumption.textures_ != &s.menu_materials->textures() ||
        &s.menu_materials->frontend_ != &s.frontend || &s.menu_materials->materials_ != &s.materials ||
        &s.menu_materials->backend_ != &reset.materials ||
        &s.menu_materials->device_ != &s.presentation.replacements() ||
        s.menu_materials->resource_epoch_ == s.materials.source_epoch_)
        throw FormatError("adapter rebind found foreign receivers or a binding already captured for this epoch");
    s.resource_transition = SourceSceneEpochTransition::rebinding_adapters;
    s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::adapters_reserved;
    try
    {
        // Every validation/allocation precedes the non-callback commit. Neither
        // constructor nor identity capture invokes a retail child or resets any
        // state. A failed attempt leaves old receivers installed but invalid.
        const auto identity = s.materials.capture_next_scene_adapter_identity(reset.materials);
        s.retired_menu_materials.reserve(s.retired_menu_materials.size() + 1);
        std::unique_ptr<SourceFrontendMaterialBinding> fresh{new SourceFrontendMaterialBinding(
            s.frontend, s.materials, reset.materials, s.presentation.replacements(), identity)};
        static_assert(std::is_nothrow_copy_constructible_v<SourceNextSceneAdapterIdentity>);
        static_assert(std::is_nothrow_copy_assignable_v<SourceNextSceneAdapterIdentity>);
        auto &textures = fresh->textures(); // inert noexcept; no new-epoch resource borrow yet
        if (s.resource_transition != SourceSceneEpochTransition::rebinding_adapters ||
            s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::adapters_reserved ||
            *s.app_callback_depth != 0 || *s.app_callbacks_retired || s.menu_lifetime_closed ||
            s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
            s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_ ||
            reset.materials.resource_epoch() != s.font_backend_epoch)
            throw FormatError("nested resource or owner change cannot commit next-scene adapters");
        s.materials.require_next_scene_adapter_identity(identity, SourceMaterialEpochPhase::adapters_reserved);
        // NEW HOST lease only after this same epoch's full source scene tail.
        // Attachment validates actual packet/vertex owners before commit.
        attach_world_polygon_level(s.frontend.world().polygon_frame);
        // Capacity is reserved, unique_ptr moves and weak-identity copies cannot
        // throw, and no callback runs here. Source words and old adapters stay
        // untouched. Publish ordinary borrowing readiness only after all stores.
        s.retired_menu_materials.push_back(std::move(s.menu_materials));
        s.menu_materials = std::move(fresh);
        language.materials_ = s.menu_materials.get();
        consumption.textures_ = &textures;
        s.frames.source_next_scene_ = identity;
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::adapters_ready;
        s.resource_transition = SourceSceneEpochTransition::adapters_ready;
    }
    catch (...)
    {
        detach_world_polygon_level(s.frontend.world().polygon_frame);
        s.materials.source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        s.resource_transition = SourceSceneEpochTransition::failed;
        throw;
    }
    // NEW host adapter completion only. Composition can now return from
    // its resource child; later race initialization/update/draw remain separate.
}
SourceOuterOutcome SourceApplicationCore::update_source_race_442810(std::uint64_t serial,
                                                                    SourceCoreRaceUpdateRequired &required,
                                                                    SceneTransitionServices &transitions,
                                                                    SourceOuterTrace *trace)
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    if (s.menu_lifetime_closed || !s.menu || !s.menu_materials ||
        s.resource_transition != SourceSceneEpochTransition::adapters_ready ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::adapters_ready ||
        s.frontend.world().scene_mode != 1)
        throw FormatError("442810 requires this Core's entered source race and committed current adapters");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_)
        throw FormatError("442810 requires idle live App owners outside native/resource callbacks");
    (void)s.menu_materials->fonts(); // actual current resource/backend/allocation identities
    const auto &clock = s.frontend.world().tick_clock;
    if (clock.increment == 0 || clock.increment > std::numeric_limits<std::uint32_t>::max() / 60u)
        throw FormatError("442810 requires an actual bounded434840 clock increment");
    const auto handles = s.native.current();
    const auto input = s.platform->window_input(handles.instance, handles.window)
                           .borrow_initialized(s.frontend.world().source_input.acquisition, handles);
    if (s.source_race_invocation_active)
        throw FormatError("source race draw/update reentry");
    s.source_update_cadence.begin(serial); // same existing duplicate/reentry guard, before source writes
    s.source_race_invocation_active = true;
    try
    {
        auto outcome = update_source_core_race_442810(s.frontend, s.counter, input, s.menu_materials->textures(),
                                                      s.samples, required, transitions, trace);
        if (outcome.status == SourceOuterStatus::completed)
            s.source_update_cadence.complete();
        else
            s.source_update_cadence.poison();
        // No post-update scene/epoch equality guard: source441DC0 may have
        // completed a real synchronous scene transition. No draw is attempted.
        s.source_race_invocation_active = false;
        return outcome;
    }
    catch (...)
    {
        s.source_update_cadence.poison();
        s.source_race_invocation_active = false;
        throw;
    }
}
SourceCoreRaceDrawKind SourceApplicationCore::draw_source_race_4427b0(std::uint64_t serial,
                                                                      SourceCoreRaceDrawRequired &required)
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    if (s.menu_lifetime_closed)
        throw FormatError("Source race draw outlived its application lifetime");
    // 4427B0..4427B7 ->442805: the source gate precedes ALL material/frame
    // borrows.44EA60 still invokes draw after update changes race to menu.
    if (s.frontend.world().scene_mode != 1)
        return SourceCoreRaceDrawKind::not_race;
    if (!s.menu || !s.menu_materials || s.resource_transition != SourceSceneEpochTransition::adapters_ready ||
        s.materials.source_epoch_phase_ != SourceMaterialEpochPhase::adapters_ready)
        throw FormatError("4427B0 requires this Core's entered source race and committed current adapters");
    if (!s.app_callback_depth || !s.app_callbacks_retired || *s.app_callback_depth != 0 || *s.app_callbacks_retired ||
        s.presentation.source_frame_active() || s.presentation.renderer_suspended() ||
        s.frames.source_renderer_reset_epoch_ || s.frames.source_material_loading_epoch_ ||
        s.source_race_invocation_active)
        throw FormatError("4427B0 requires idle live App owners outside source/native/resource callbacks");
    (void)s.menu_materials->fonts(); // actual current resource/backend/allocation identities
    (void)capture_world_polygon_level(s.frontend.world().polygon_frame); // no opportunistic reattachment
    auto submit = [&] {
        // Resolve at44260B, after all preceding source callbacks. No stale
        // image table or material adapter is retained across a scene change.
        if (!s.menu_materials)
            throw FormatError("source draw lost its current material adapter");
        const auto &images = s.menu_materials->current_images();
        SourceWorldSubmissionContext context{s.frontend,
                                             s.frames,
                                             images,
                                             s.submission,
                                             s.presentation.sink(),
                                             s.draw_failure,
                                             desktop_polygon_submission_budget};
        submit_source_world_polygons_433f40(context);
    };
    CoreDrawChildren children(required, submit);
    SourceCoreRaceDrawContext context{
        s.frontend, s.frames, s.submission, s.presentation.sink(), s.presentation.device(), s.source_gamma_5da1dc,
        children};
    s.source_draw_cadence.begin(serial);
    s.source_race_invocation_active = true;
    try
    {
        const auto outcome = draw_source_core_race_4427b0(context);
        s.source_draw_cadence.complete();
        s.source_race_invocation_active = false;
        return outcome;
    }
    catch (...)
    {
        s.source_draw_cadence.poison();
        s.source_race_invocation_active = false;
        detach_world_polygon_level(s.frontend.world().polygon_frame);
        s.presentation.discard_failed_frame(); // capture only; no synthetic source EndScene/rollback
        throw;
    }
}
SessionOuterFrameServices &SourceApplicationCore::frames()
{
    return impl_->frames;
}
SourceMenuPresentationBackend &SourceApplicationCore::presentation()
{
    return impl_->presentation;
}
FrontendPerformanceCounter &SourceApplicationCore::counter()
{
    return impl_->counter;
}
SourceColdTimePlatform &SourceApplicationCore::time()
{
    return impl_->time;
}
SourceBootFilePlatform &SourceApplicationCore::files()
{
    return impl_->files;
}
SourceApplicationWindowClock &SourceApplicationCore::window_clock()
{
    return impl_->window_clock;
}
SourceColdSceneStartupContext SourceApplicationCore::cold_scene_context()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    return {{s.platform, s.materials, s.files},
            {s.frontend, s.materials, s.application, s.submission, s.production},
            &s.loading,
            nullptr};
}
InputWindowHandles SourceApplicationCore::native_window_handles() const
{
    impl_->platform->require_owner_thread();
    return impl_->native.current();
}
std::unique_ptr<SourceApplicationFinalShutdown> SourceApplicationCore::make_final_shutdown_adapter()
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    return std::make_unique<SourceApplicationFinalShutdown>(s.frontend, s.production, s.bridge, s.native.window,
                                                            s.renderer_slot);
}
SourceMainMenuChildren &SourceApplicationCore::attach_menu_after_device(SourceSavePageServices &save,
                                                                        SourceMainMenuRequiredServices &required,
                                                                        NativeSourceMenuCrt &crt,
                                                                        SourceApplicationShutdownServices &shutdown)
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    if (s.menu_lifetime_closed)
        throw FormatError("Application menu lifetime has ended; reattachment is forbidden");
    if (s.saved_shutdown != &shutdown)
        throw FormatError("Application menu attachment requires its claimed Composition shutdown receiver");
    if (s.menu)
    {
        if (s.saved_save != &save || s.saved_required != &required || s.saved_crt != &crt ||
            s.saved_shutdown != &shutdown)
            throw FormatError("Application menu services cannot be replaced");
        return *s.menu;
    }
    // Reject a foreign CRT before any input/material/menu publication.
    crt.require_platform(*s.platform);
    s.startup.require_gpu_admitted();
    const auto handles = s.native.current();
    auto input = s.platform->window_input(handles.instance, handles.window)
                     .borrow_for_menu_construction(s.frontend.world().source_input.acquisition, handles);
    auto device = s.production.reset_context(s.frontend); // borrow only, no reset
    s.menu_materials = std::make_unique<SourceFrontendMaterialBinding>(s.frontend, s.materials, device.materials,
                                                                       s.presentation.replacements());
    s.menu = std::make_unique<SourceMainMenuChildren>(s.frontend, crt, shutdown, input.keyboard, input.direct_input,
                                                      s.music, s.frames, s.presentation.sink(), *s.menu_materials,
                                                      s.character_62ddfc, s.counter, s.samples, save, required);
    s.saved_save = &save;
    s.saved_required = &required;
    s.saved_crt = &crt;
    s.saved_shutdown = &shutdown;
    return *s.menu;
}
SourceMainMenuChildren &SourceApplicationCore::menu()
{
    impl_->platform->require_owner_thread();
    if (impl_->menu_lifetime_closed)
        throw FormatError("Application menu lifetime has ended");
    if (!impl_->menu)
        throw FormatError("Application menu has not been attached after device production");
    return *impl_->menu;
}
void SourceApplicationCore::release_menu_after_completed_shutdown(SourceApplicationShutdownServices &shutdown,
                                                                  const SourceApplicationHostCallbackState &callbacks)
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    if (s.menu_lifetime_closed)
        throw FormatError("Application menu lifetime has already ended");
    if (!s.menu || s.saved_shutdown != &shutdown || !shutdown.new_host_shutdown_completed_for(s.frontend))
        throw FormatError("Menu lifetime close requires its actual completed442180 receiver");
    if (callbacks.depth != 0 || !callbacks.retired)
        throw FormatError("Menu lifetime close requires retired App callbacks outside native dispatch");
    // Retain all source words and service identities. Close inert C++ borrowers
    // while Composition, Save, Required and CRT are still alive. No source
    // destructor is rerun and no empty slot may masquerade as a cold restart.
    s.menu_lifetime_closed = true;
    s.menu.reset();
    s.menu_materials.reset();
}
void SourceApplicationCore::bind_menu_lifetime_owner(SourceApplicationShutdownServices &shutdown,
                                                     const SourceApplicationHostCallbackState &callbacks)
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    if (s.menu_lifetime_closed || s.saved_shutdown)
        throw FormatError("Application menu lifetime already has a Composition owner");
    s.saved_shutdown = &shutdown;
    s.app_callback_depth = &callbacks.depth;
    s.app_callbacks_retired = &callbacks.retired;
}
void SourceApplicationCore::abandon_menu_adapters(SourceApplicationShutdownServices &shutdown,
                                                  const SourceApplicationHostCallbackState &callbacks)
{
    auto &s = *impl_;
    s.platform->require_owner_thread();
    if (s.menu_lifetime_closed)
        throw FormatError("Application menu lifetime has already ended");
    if (s.saved_shutdown != &shutdown)
        throw FormatError("Menu abandonment requires its actual Composition owner");
    if (callbacks.depth != 0 || !callbacks.retired)
        throw FormatError("Menu abandonment requires retired App callbacks outside native dispatch");
    if (shutdown.new_host_shutdown_progress() == SourceApplicationShutdownProgress::in_progress)
        throw FormatError("Menu abandonment cannot destroy an active source shutdown borrower");
    // Both objects are inert C++ receivers. Their destructors do not invoke
    // source page callbacks, texture uploads, GPU waits or input release.
    // A partially constructed material binding without a menu is also owned
    // here. Keep every reached source word/resource and the shutdown observation.
    s.menu_lifetime_closed = true;
    s.menu.reset();
    s.menu_materials.reset();
}
} // namespace sgn::retail
