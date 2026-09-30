#pragma once
#include "sgn/source_application_entry.hpp"
#include "sgn/retail_scene_transition.hpp"
#include <functional>
#include <memory>
#include <cstddef>

namespace sgn::retail
{
class SourceApplicationCore;
class SourceApplicationMenuServices;
class SourceMainMenuChildren;
class NativeSourceMenuCrt;
struct SourceApplicationShutdownServices;
struct PersistenceServices;
struct SourceFrontendDataStartup;
struct SourceSavePageServices;
struct SourceMainMenuRequiredServices;

// NEW HOST live borrows, not readiness flags owned by Composition. Pass the
// actual App.window_callback_depth and App.source_callbacks_retired lvalues;
// both must outlive this composition. Non-const lvalue parameters forbid
// temporary snapshots. This API does not prove the caller chose the App slots.
struct SourceApplicationHostCallbackState
{
    const std::size_t &depth;
    const bool &retired;
    SourceApplicationHostCallbackState(std::size_t &app_depth, bool &app_retired) noexcept
        : depth(app_depth), retired(app_retired)
    {
    }
};

// NEW HOST application-lifetime composition. Core and all supplied receivers
// outlive this object. Save/Required/CRT also outlive every use of the actual
// menu retained in Core. Retire callbacks and finish menu use before destroying
// this object. Destroy Save/Required/CRT afterwards, while Core is still alive:
// native Save also borrows Core. Final App integration must call the explicit
// finish_host_lifetime boundary while all receivers still exist. No menu use may follow
// destruction of this composition or any supplied receiver.
// It owns only adapters, never another world/device/menu.
// Construction is inert and may precede HWND creation. The existing44EA60
// dispatcher is entered only by run(), after the App has created its HWND.
//
// The supplied Native CRT must be the existing App/Core-bound owner and
// outlive this composition and every menu callback. No CRT is created here.
// Remaining scene services supply later warm/race operations.
// Save/Required/entry receivers remain mandatory, with no success defaults.
class SourceApplicationMenuComposition final : public SourceApplicationEntryServices, public SceneTransitionServices
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    SourceApplicationMenuComposition(
        SourceApplicationCore &, NativeSourceMenuCrt &actual_core_crt, SourceApplicationEntryServices &remaining_entry,
        SceneTransitionServices &remaining_scene, PersistenceServices &, SourceFrontendDataStartup &,
        SourceSavePageServices &, SourceMainMenuRequiredServices &,
        SourceApplicationHostCallbackState actual_app_callbacks,
        // Mandatory App::bind_source_menu_window with the existing Core words.
        // Called once after three actual attachments, before source43D0B0.
        std::function<void(SourceApplicationMenuServices &, SourceMainMenuChildren &)> publish_menu_callbacks,
        // Mandatory App hook: retire_source_callbacks_for_final_shutdown().
        // Must reject entry from an active native callback before retirement.
        // Its captured App lifetime outlives this composition. Never a no-op.
        std::function<void()> retire_source_callbacks);
    ~SourceApplicationMenuComposition() override;
    SourceApplicationMenuComposition(const SourceApplicationMenuComposition &) = delete;
    SourceApplicationMenuComposition &operator=(const SourceApplicationMenuComposition &) = delete;

    SourceApplicationEntryResult run(const SourceApplicationEntryArguments &, SourceApplicationMessages &);
    // Only available after this owner has published its actual Core menu to
    // all three receivers. Early WNDPROC continues to use Core.surface_bridge.
    SourceApplicationMenuServices &menu_services();
    // Same shutdown receiver used by44EA60, exposed for the actual CRT exit
    // composition. This abstract borrow cannot attach different page/input owners.
    SourceApplicationShutdownServices &shutdown_services();
    // Explicit NEW HOST boundary after run returned or unwound; never call from
    // native dispatch. Retires callbacks, then ends Core's actual menu C++
    // lifetime by exactly one terminal route:
    // - 442180 already completed (44EB53): close the menu, nothing replayed.
    // - entry unwound (SourceHostProcessExit or any exception) after 442120
    //   returned and 442180 never started: run the full 442180 ladder once,
    //   as the original fatal path 4234D0 does before exit(1), then close.
    // - 44EAAD direct return or an entry unwound before 442120 returned
    //   (original: no 442180, 44E4A0 is exit(1) only), or a 442180 interrupted
    //   by the unwinding exception (partial, never replayed): close only the
    //   inert adapters, like abandon_host_menu_lifetime. Not an error.
    // In-progress, or a returned entry that skipped 44EB53, is a contract error.
    // One attempt only, including failure. Never catches/translates the original
    // exit, clears App.window_failure or certifies success: the caller must keep
    // its first outcome and inspect that latch again after this method returns.
    // An exception from the 442180 children is a separate cleanup failure.
    void finish_host_lifetime();
    // Terminal fallback when finish_host_lifetime itself failed.
    // Requires the actual App to have ALREADY retired its callbacks at depth0;
    // never calls the retirement hook or442180 again. Closes only Core's inert
    // menu/material adapter lifetimes, including failed startup before menu
    // attachment. Leaves source/GPU/input cleanup status and all source stores
    // unchanged. Core must remain alive until providers/CRT are destroyed.
    // No active run or shutdown may borrow the adapters. Also terminal before
    // run: once abandoned, run/reattach/finish are forbidden. Preserve primary
    // and cleanup exceptions outside this method; it does not consume a latch.
    void abandon_host_menu_lifetime();

    void show_cursor(bool) override;
    std::uint32_t initialize_window_44e890(const SourceApplicationEntryArguments &) override;
    void invoke(SourceApplicationCall) override;
    void perform(SceneOperation) override;
    void select_audio_environment(std::uint32_t) override;
    void initialize_menu_viewport(std::uint32_t) override;
    void reset_tick_clock() override;
};
} // namespace sgn::retail
