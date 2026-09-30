#include "sgn/source_application_menu_composition.hpp"
#include "sgn/host_profiler.hpp"
#include <optional>
#include "sgn/source_scene_crt_power.hpp"
#include "sgn/source_cold_vehicle_state.hpp"
#include "sgn/source_vehicle_bootstrap.hpp"
#include "sgn/source_normal_race_initialize.hpp"
#include "sgn/source_application_core.hpp"
#include "sgn/source_native_vehicle_update.hpp"
#include "sgn/source_core_race_draw_remaining.hpp"
#include "sgn/source_application_final_shutdown.hpp"
#include "sgn/source_application_menu_services.hpp"
#include "sgn/source_application_surfaces_late_bound.hpp"
#include "sgn/source_cold_menu_scene_services.hpp"
#include "sgn/source_menu_presentation_backend.hpp"
#include "sgn/source_scene_reset_composition.hpp"
#include "sgn/source_outer_scheduler.hpp"
#include "sgn/source_score_configuration.hpp"
#include "sgn/source_gameplay_configuration.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/source_menu_crt_exit.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include <utility>
#include <limits>

namespace sgn::retail
{
namespace
{
SourceApplicationCore &require_core_crt_pair(SourceApplicationCore &core, NativeSourceMenuCrt &crt)
{
    crt.require_platform(*core.platform());
    return core;
}
// All entry and CRT shutdown paths share this concrete host lifetime boundary.
// The source menu gate may skip close_menu, so every first reached operation
// requires retirement. The hook must finish before any release starts.
class RetiringShutdown final : public SourceApplicationShutdownServices
{
    SourceApplicationCore &core_;
    SourceApplicationFinalShutdown &actual_;
    SourceApplicationHostCallbackState callbacks_;
    std::function<void()> retire_;
    bool retired_{}, menu_lifetime_closed_{};
    void require_retired()
    {
        core_.platform()->require_owner_thread();
        if (menu_lifetime_closed_)
            throw FormatError("Application shutdown receivers cannot outlive the closed menu lifetime");
        if (callbacks_.depth != 0)
            throw FormatError("Application shutdown cannot retire live native callbacks");
        if (!retired_)
        {
            retire_(); // Actual App hook; returning alone is not proof of retirement.
            if (callbacks_.depth != 0 || !callbacks_.retired)
                throw FormatError("App callback retirement did not publish its terminal state");
            retired_ = true;
        }
        if (!callbacks_.retired)
            throw FormatError("App callback retirement was revoked");
    }

  public:
    RetiringShutdown(SourceApplicationCore &core, SourceApplicationFinalShutdown &actual,
                     SourceApplicationHostCallbackState callbacks, std::function<void()> retire)
        : core_(core), actual_(actual), callbacks_(callbacks), retire_(std::move(retire))
    {
        if (!retire_)
            throw FormatError("Application menu composition requires the App callback-retirement hook");
    }
    void retire_for_host_completion()
    {
        require_retired();
    }
    void mark_menu_lifetime_closed() noexcept
    {
        menu_lifetime_closed_ = true;
    }
    void close_menu_43d180() override
    {
        require_retired();
        actual_.close_menu_43d180();
    }
    void release_graphics_resources_431800() override
    {
        require_retired();
        actual_.release_graphics_resources_431800();
    }
    void release_display_422a60() override
    {
        require_retired();
        actual_.release_display_422a60();
    }
    void shutdown_input_424930() override
    {
        require_retired();
        actual_.shutdown_input_424930();
    }
};
} // namespace
struct SourceApplicationMenuComposition::Impl
{
    SourceApplicationCore &core;
    NativeSourceMenuCrt &crt;
    SourceApplicationHostCallbackState callbacks;
    std::unique_ptr<SourceApplicationFinalShutdown> shutdown;
    RetiringShutdown retiring_shutdown;
    SourceColdMenuSceneServices cold_scene;
    SourceColdInitializationContext cold_initialization;
    SourceApplicationMenuServices entry;
    SourceSavePageServices &save;
    SourceMainMenuRequiredServices &required;
    std::function<void(SourceApplicationMenuServices &, SourceMainMenuChildren &)> publish_menu_callbacks;
    bool entered{}, finished{}, failed{}, resources_started{}, menu_attached{};
    bool cold_initialization_returned{}, host_finish_started{};
    bool host_menu_lifetime_closed{}, host_finish_active{};
    // NEW host invocation identity, never a simulation tick or display rate.
    // R17: the concrete 418A90 receiver borrows this Core's platform, CRT,
    // frontend and sample owners. Unbound children still throw at their site.
    SourceNativeVehicleUpdate race_update; // R18 unified game: native 418A90 on the single world
    // R17: concrete4473F0/449FB0/40E030 receiver on this Core and its bound CRT.
    SourceCoreRaceDrawRemaining race_draw;
    std::uint64_t source_race_serial{};
    bool source_race_draw_pending{};

    Impl(SourceApplicationMenuComposition &owner, SourceApplicationCore &application, NativeSourceMenuCrt &actual_crt,
         SourceApplicationEntryServices &remaining_entry, SceneTransitionServices &remaining_scene,
         PersistenceServices &persistence, SourceFrontendDataStartup &data, SourceSavePageServices &save_receiver,
         SourceMainMenuRequiredServices &required_receiver, SourceApplicationHostCallbackState actual_callbacks,
         std::function<void(SourceApplicationMenuServices &, SourceMainMenuChildren &)> publish,
         std::function<void()> retire)
        : core(require_core_crt_pair(application, actual_crt)), crt(actual_crt), callbacks(actual_callbacks),
          shutdown(core.make_final_shutdown_adapter()),
          retiring_shutdown(core, *shutdown, callbacks, std::move(retire)),
          cold_scene(core.cold_scene_context(), core.source_gamma_publication(), persistence, data, remaining_scene),
          cold_initialization{core.frontend(), core.time(), core.counter(), core.files(), {}, owner},
          entry(remaining_entry, core.frontend(), core.counter(), owner, core.frames(), core.presentation().sink(),
                core.presentation().device(), core.files(), core.window_clock(), core.surface_bridge(),
                cold_initialization, retiring_shutdown, core.window_startup()),
          save(save_receiver), required(required_receiver), publish_menu_callbacks(std::move(publish)),
          race_update(*core.platform()), race_draw(core, crt)
    {
        if (!publish_menu_callbacks)
            throw FormatError("Application menu composition requires the App menu-publication hook");
    }

    void require_running() const
    {
        core.platform()->require_owner_thread();
        if (!entered || finished || failed)
            throw FormatError("Application menu composition is outside its one active source entry");
    }
    // NEW HOST terminal close of the inert menu/material adapters when no
    // 442180 may run (see finish_host_lifetime). Callbacks are already retired
    // by the App hook; source words, reached resources and the shutdown
    // observation stay unchanged. Core validates owner, depth and retirement.
    void close_adapters_without_source_cleanup()
    {
        core.abandon_menu_adapters(retiring_shutdown, callbacks);
        retiring_shutdown.mark_menu_lifetime_closed();
        host_menu_lifetime_closed = true;
    }
};

SourceApplicationMenuComposition::SourceApplicationMenuComposition(
    SourceApplicationCore &core, NativeSourceMenuCrt &actual_crt, SourceApplicationEntryServices &entry,
    SceneTransitionServices &scene, PersistenceServices &persistence, SourceFrontendDataStartup &data,
    SourceSavePageServices &save, SourceMainMenuRequiredServices &required,
    SourceApplicationHostCallbackState callbacks,
    std::function<void(SourceApplicationMenuServices &, SourceMainMenuChildren &)> publish,
    std::function<void()> retire)
    : impl_(std::make_unique<Impl>(*this, core, actual_crt, entry, scene, persistence, data, save, required, callbacks,
                                   std::move(publish), std::move(retire)))
{
    // Last construction step: no later throwing work can leave this claim
    // pointing at an unsuccessfully constructed Composition. Core remains the
    // owner of any future menu/material adapters, even if attachment fails.
    impl_->core.bind_menu_lifetime_owner(impl_->retiring_shutdown, impl_->callbacks);
}
SourceApplicationMenuComposition::~SourceApplicationMenuComposition() = default;

SourceApplicationEntryResult SourceApplicationMenuComposition::run(const SourceApplicationEntryArguments &arguments,
                                                                   SourceApplicationMessages &messages)
{
    auto &s = *impl_;
    s.core.platform()->require_owner_thread();
    if (s.entered || s.host_menu_lifetime_closed)
        throw FormatError("Application source entry cannot be restarted after reached effects or abandonment");
    s.entered = true;
    try
    {
        // Current identities from the actual retained App slot, acquired after
        // native creation. No second HWND, Renderer or input owner is created.
        const auto handles = s.core.native_window_handles();
        if (!handles.instance || !handles.window || arguments.instance != handles.instance)
            throw FormatError("Application source entry requires its actual existing HWND/HINSTANCE");
        s.cold_initialization.input_handles = handles;
        const auto words = s.core.window_words();
        s.core.window_startup().require_entry_words(words.active_61c374, words.mode_61c364);
        auto &w = s.core.frontend().world();
        SourceApplicationEntryWorld world{
            w.boot_config, w.outer, w.scene_mode, w.options.selection_phase, words.active_61c374, words.mode_61c364};
        const auto result = run_source_application_44ea60(world, arguments, *this, messages);
        s.finished = true;
        return result;
    }
    catch (const SourceHostProcessExit &)
    {
        s.failed = true;
        throw; // Keep the original status/forced flag for the outer App owner.
    }
    catch (...)
    {
        // NEW HOST terminal guard. Retain every reached source store/resource;
        // a retry must not repeat442120 or skip a partly initialized43D0B0.
        s.failed = true;
        throw;
    }
}
SourceApplicationMenuServices &SourceApplicationMenuComposition::menu_services()
{
    auto &s = *impl_;
    s.require_running();
    if (!s.menu_attached)
        throw FormatError("Full menu callbacks require completed actual menu attachment");
    return s.entry;
}
SourceApplicationShutdownServices &SourceApplicationMenuComposition::shutdown_services()
{
    impl_->core.platform()->require_owner_thread();
    return impl_->retiring_shutdown;
}
void SourceApplicationMenuComposition::finish_host_lifetime()
{
    auto &s = *impl_;
    s.core.platform()->require_owner_thread();
    if (s.host_menu_lifetime_closed)
        throw FormatError("Application menu adapter lifetime has already ended");
    if (!s.entered || (!s.finished && !s.failed))
        throw FormatError("Host menu lifetime close requires returned or unwound source entry");
    if (s.host_finish_started)
        throw FormatError("Host menu lifetime close cannot be repeated after reached effects");
    if (s.callbacks.depth != 0)
        throw FormatError("Host menu lifetime close requires the outer App boundary");
    struct FinishBorrow
    {
        bool &active;
        explicit FinishBorrow(bool &value) noexcept : active(value)
        {
            active = true;
        }
        ~FinishBorrow() noexcept
        {
            active = false;
        }
    } finish_borrow{s.host_finish_active};
    s.host_finish_started = true;
    s.retiring_shutdown.retire_for_host_completion();
    switch (s.retiring_shutdown.new_host_shutdown_progress())
    {
    case SourceApplicationShutdownProgress::never_started:
        if (!s.cold_initialization_returned)
        {
            // 44EAAD direct return after 44E890==0, or an entry unwound before
            // 442120 returned. The original reaches no 442180 on either path:
            // 44EAAD returns before 44EB53, and its startup fatal 44E4A0 is only
            // PUSH 1; CALL 462DFA (exit(1), no 442180). Here the 442180
            // receivers (menu pages, material bank) may not exist yet, so the
            // host closes only its inert adapters; App/Core destructors release
            // window/GPU owners afterwards. This is the orderly terminal route,
            // not a second failure: the caller retains the entry outcome.
            s.close_adapters_without_source_cleanup();
            return;
        }
        if (!s.failed)
            throw FormatError("Returned source entry after 442120 cannot have skipped its 44EB53 cleanup");
        // NEW HOST (R22 G3) terminal cleanup after an entry that unwound after
        // 442120 returned: SourceHostProcessExit (PageMain 4532A1 -> 462DFA)
        // or a host/boundary exception anywhere later, including in a race
        // or during a scene unload/load. Modelled on the original fatal path
        // 4234D0: CALL 442180 (4234D0), PUSH 1; CALL 462DFA (4234D5/4234D7),
        // reached mid-load from 42EFB0 (42F038), 45E8E0 (45E903), 45E9B0
        // (45E9E0) and from 4622C0/4623C0/462440. The full ladder therefore
        // runs in the original order 43D180 (if 60D61C==1), 431800, 44B1A0,
        // 422A60, 424930, 4621D0, 42EAF0 on whatever scene state was reached.
        // All source frames have unwound and callbacks are retired (above).
        // Called once; an exception propagates to the caller as a separate
        // cleanup failure and never replaces the retained first failure.
        shutdown_source_application_442180(s.core.frontend(), s.retiring_shutdown);
        break;
    case SourceApplicationShutdownProgress::completed:
        break; // The original full cleanup already returned; do not replay it.
    case SourceApplicationShutdownProgress::partial:
        // An earlier 442180 (44EB53 or the CRT exit callback) was interrupted
        // by the exception that unwound this entry; that exception is the
        // caller's retained first failure. 442180 children are not idempotent,
        // so never replay them. Close only the inert adapters.
        if (!s.failed)
            throw FormatError("Interrupted source cleanup requires the unwound entry that interrupted it");
        s.close_adapters_without_source_cleanup();
        return;
    case SourceApplicationShutdownProgress::in_progress:
        throw FormatError("Active source cleanup cannot be finished from its own borrower");
    }
    s.core.release_menu_after_completed_shutdown(s.retiring_shutdown, s.callbacks);
    s.retiring_shutdown.mark_menu_lifetime_closed();
    s.host_menu_lifetime_closed = true;
}
void SourceApplicationMenuComposition::abandon_host_menu_lifetime()
{
    auto &s = *impl_;
    s.core.platform()->require_owner_thread();
    if (s.host_menu_lifetime_closed)
        throw FormatError("Application menu adapter lifetime has already ended");
    if (s.entered && !s.finished && !s.failed)
        throw FormatError("Cannot abandon menu adapters while source entry is active");
    if (s.host_finish_active)
        throw FormatError("Cannot abandon menu adapters from an active host cleanup hook");
    // Do not retry a potentially failed retirement hook. The actual App must
    // establish terminal retirement before calling this separate fallback.
    s.core.abandon_menu_adapters(s.retiring_shutdown, s.callbacks);
    s.retiring_shutdown.mark_menu_lifetime_closed();
    s.host_menu_lifetime_closed = true;
}
void SourceApplicationMenuComposition::show_cursor(bool visible)
{
    auto &s = *impl_;
    s.require_running();
    s.entry.show_cursor(visible);
}
std::uint32_t SourceApplicationMenuComposition::initialize_window_44e890(
    const SourceApplicationEntryArguments &arguments)
{
    auto &s = *impl_;
    s.require_running();
    const auto result = s.entry.initialize_window_44e890(arguments);
    if (result != 0)
    {
        // NEW HOST lifetime attachment after the actual44E890 host adoption.
        // Does not run4249B0: the original caller still owns that at442161.
        s.shutdown->bind_input(s.cold_initialization.input_handles);
    }
    return result;
}
void SourceApplicationMenuComposition::invoke(SourceApplicationCall call)
{
    auto &s = *impl_;
    s.require_running();
    if (call == SourceApplicationCall::update_race_442810)
    {
        if (s.source_race_draw_pending || s.source_race_serial == std::numeric_limits<std::uint64_t>::max())
            throw FormatError("Application race invocation is unmatched or its host serial exhausted");
        const auto serial = ++s.source_race_serial;
        std::optional<SourceOuterOutcome> profiled;
        {
            SGN_PROFILE_SCOPE("pass.update_442810");
            profiled = s.core.update_source_race_442810(serial, s.race_update, *this);
        }
        const auto result = *profiled;
        if (result.status != SourceOuterStatus::completed)
            throw SourceCoreRaceUpdateBoundary(
                0x44eb25, result.blocked_address,
                "Application race update stopped at a required source child before completion");
        s.source_race_draw_pending = true;
        return;
    }
    if (call == SourceApplicationCall::draw_race_4427b0)
    {
        if (!s.source_race_draw_pending)
            throw FormatError("Application race draw requires its immediately completed source update");
        s.source_race_draw_pending = false; // reserve before any source draw effect
        // 44EA60 calls this even if441DC0 changed the scene during update.
        // Core honors4427B0's initial scene gate before borrowing any resources.
        {
            SGN_PROFILE_SCOPE("pass.draw_4427b0");
            (void)s.core.draw_source_race_4427b0(s.source_race_serial, s.race_draw);
        }
        sgn::host_profile::frame_boundary();
        return;
    }
    s.entry.invoke(call);
    // This observes the actual call return, including43D0B0 and the remaining
    // 442030 tail. Menu attachment/60D61C/PeekMessage cannot establish it.
    if (call == SourceApplicationCall::initialize_scene_442120)
        s.cold_initialization_returned = true;
}
void SourceApplicationMenuComposition::perform(SceneOperation operation)
{
    auto &s = *impl_;
    s.require_running();
    // 44204E is BEFORE442053 ->42E6E1. Observe the actual next-epoch bank
    // begin through Core, never insert it after42E510 or replay it in load.
    // Cold construction keeps its existing prefix path. Subsequent loads
    // consume this actual bank-begin transition in the ordered Core chain.
    if (operation == SceneOperation::begin_allocations &&
        s.core.source_scene_epoch_transition() != SourceSceneEpochTransition::cold)
    {
        s.core.begin_next_source_scene_allocations_42ea80();
        return;
    }
    // Existing concrete source children, same Core world and QPC owner.
    // Audio children preserve actual failures;445FA0 uses its complete
    // same-world reset, two QPC reads and rail-reset tail before44204E.
    const auto prefix = perform_source_scene_initialization_prefix(operation, *s.core.platform(), s.core.counter());
    if (prefix.status == SourceScenePrefixStatus::completed)
        return;
    if (prefix.status == SourceScenePrefixStatus::child_required)
        throw SourceColdSceneObservation{prefix.blocked_address, prefix.required_identity};
    if (operation == SceneOperation::release_image_resources)
    {
        // 441DC0 ->4420E0 ->431810, before42EAA0 and before the scene store.
        // Core's existing presentation owns this SAME current-epoch binding.
        s.core.release_source_menu_images_431810();
        return;
    }
    if (operation == SceneOperation::release_allocations)
    {
        // 4420E5 ->42EAA0 ->42EA00 on the same bank used by cold producers.
        // No owner replacement, forced persistent release or bank reinitialization.
        s.race_update.release_scene_collision(); // decoded copy of the released level (NEW host)
        s.core.release_source_scene_allocations_42eaa0();
        return;
    }
    if (operation == SceneOperation::ensure_score_configuration)
    {
        ensure_source_score_configuration_443d70(*s.core.platform(), s.core.files());
        return;
    }
    if (operation == SceneOperation::ensure_gameplay_configuration)
    {
        ensure_source_gameplay_configuration_4457e0(*s.core.platform(), s.core.files(), s.crt);
        return;
    }
    if (operation == SceneOperation::load_scene_resources)
    {
        if (s.resources_started)
        {
            if (!s.menu_attached)
                throw FormatError("Scene reload requires this Composition's completed initial menu attachment");
            // 42EA80 already ran at44204E. Execute each reached42E6E0 slice
            // once on Core's retained owners; every stage validates the prior
            // epoch transition and retains partial source/GPU effects on error.
            s.core.reset_next_source_scene_epoch_42e510(); // 42E6E1
            s.core.load_next_source_level_script_42e6e6();
            s.core.prepare_next_source_scene_models_42e6eb();
            s.core.prepare_next_source_scene_renderer_42e6fa();
            s.core.load_next_source_scene_materials_42e744();
            s.core.initialize_next_source_scene_fonts_42e74e();
            s.core.prepare_next_source_scene_storage_42e778();
            s.core.initialize_next_source_camera_42e787();
            s.core.finish_next_source_scene_tail_42e791(); // through42E7F8
            // NEW host publication after the complete source child returns.
            // Keep the same menu, CRT, callback slots and shutdown receiver;
            // retired adapters retain their old epoch and remain invalid.
            s.core.rebind_next_source_scene_adapters();
            return;
        }
        s.resources_started = true;
        // Reached by442030 CALL42E6E0 at442053. The full recovered child must
        // return, including real42FD80 GPU admission, before menu construction.
        s.cold_scene.perform(operation);
        auto &menu = s.core.attach_menu_after_device(s.save, s.required, s.crt, s.retiring_shutdown);
        // These adapters were constructed together from the same Core and are
        // not exposed for menu attachment beforehand. Each publication invokes
        // its existing owner/GPU guards; exceptions make the enclosing run terminal.
        s.entry.attach_menu(menu);
        s.cold_scene.attach_menu(menu, menu);
        s.shutdown->attach_pages(menu);
        s.publish_menu_callbacks(s.entry, menu); // actual App WNDPROC publication, not a source initializer
        s.menu_attached = true;                  // NEW host attachment only, never source60D61C.
        return;
    }
    if (operation == SceneOperation::initialize_vehicle_state)
    {
        initialize_source_cold_vehicle_state_40a2f0(*s.core.platform());
        return;
    }
    if (operation == SceneOperation::initialize_vehicle_presentation)
    {
        initialize_source_world_frame_globals_401de0(*s.core.platform());
        return;
    }
    if (operation == SceneOperation::initialize_vehicle_models)
    {
        // 442084 after401DE0; existing source bootstrap owns40BDE0/4130D0.
        initialize_source_vehicles_413670(s.core.frontend(), s.core.files(), s.crt);
        return;
    }
    if (operation == SceneOperation::initialize_race)
    {
        // 442093 follows colors/replay;40D500 owns its config and grid calls.
        initialize_source_normal_race_40d500(s.core.frontend(), s.core.files(), s.crt, s.core.native_window_handles());
        return;
    }
    if (perform_source_race_scene_small_child_with_crt_power(operation, s.core.frontend(), s.core.files(), s.crt))
        return;
    // initialize_scene still calls443D70 and4457E0 before the menu predicate,
    // and stores60D61C itself at4420B4 before invoking43D0B0 at4420BE.
    s.cold_scene.perform(operation);
}
void SourceApplicationMenuComposition::select_audio_environment(std::uint32_t value)
{
    auto &s = *impl_;
    s.require_running();
    // 4420EC ignores the source EAX, including real native query/support failure.
    // A missing owner is a boundary, never an invented completed native call.
    const auto selected = select_source_audio_environment_460af0(*s.core.platform(), value);
    if (selected.status != SourceAudioEnvironmentStatus::completed)
        throw SourceColdSceneObservation{selected.blocked_address, selected.required_spatial.value};
}
void SourceApplicationMenuComposition::initialize_menu_viewport(std::uint32_t value)
{
    auto &s = *impl_;
    s.require_running();
    s.cold_scene.initialize_menu_viewport(value);
}
void SourceApplicationMenuComposition::reset_tick_clock()
{
    auto &s = *impl_;
    s.require_running();
    reset_platform_source_clock_442100(*s.core.platform(), s.core.counter());
}
} // namespace sgn::retail
