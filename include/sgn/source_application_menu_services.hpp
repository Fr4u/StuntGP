#pragma once
#include "sgn/source_application_entry.hpp"
#include "sgn/source_application_window_messages.hpp"
#include "sgn/source_application_surfaces.hpp"
#include "sgn/source_cold_initialize.hpp"
#include "sgn/source_application_shutdown.hpp"
#include "sgn/source_application_window_initialize.hpp"
#include "sgn/source_menu_frame.hpp"
#include "sgn/session_outer_frame_services.hpp"

namespace sgn::retail
{
class SourceMenuNavigationBindingChildren;
class NewHostD3D12WindowStartup;

// Application composition, not a second frontend or source dispatcher.
// This binds the two menu calls of 44EA60 to their recovered implementations.
// The remaining startup/race/shutdown operations retain mandatory bindings.
class SourceApplicationMenuServices final : public SourceApplicationEntryServices
{
    SourceApplicationEntryServices &remaining_;
    FrontendState &frontend_;
    FrontendPerformanceCounter &counter_;
    SourceMenuFrameChildren *menu_{}; // borrowed actual menu; null only before explicit NEW HOST attach
    SceneTransitionServices &transitions_;
    SessionOuterFrameServices &frames_;
    PolygonSubmissionSink &sink_;
    LoadingFrameDevice &device_;
    SourceBootFilePlatform &boot_files_;
    SourceApplicationWindowClock &window_clock_;
    SourceApplicationSurfaceServices &surfaces_;
    SourceColdInitializationContext &cold_;
    SourceApplicationShutdownServices &shutdown_;
    SourceWindowStartupContext *window_startup_{};
    NewHostD3D12WindowStartup *d3d12_window_startup_{};

  public:
    SourceApplicationMenuServices(SourceApplicationEntryServices &, FrontendState &, FrontendPerformanceCounter &,
                                  SourceMenuFrameChildren &, SceneTransitionServices &, SessionOuterFrameServices &,
                                  PolygonSubmissionSink &, LoadingFrameDevice &, SourceBootFilePlatform &,
                                  SourceApplicationWindowClock &, SourceApplicationSurfaceServices &,
                                  SourceColdInitializationContext &, SourceApplicationShutdownServices &,
                                  SourceWindowStartupContext &);
    // Explicit NEW HOST branch. No legacy startup context or COM placeholders.
    SourceApplicationMenuServices(SourceApplicationEntryServices &, FrontendState &, FrontendPerformanceCounter &,
                                  SourceMenuFrameChildren &, SceneTransitionServices &, SessionOuterFrameServices &,
                                  PolygonSubmissionSink &, LoadingFrameDevice &, SourceBootFilePlatform &,
                                  SourceApplicationWindowClock &, SourceApplicationSurfaceServices &,
                                  SourceColdInitializationContext &, SourceApplicationShutdownServices &,
                                  NewHostD3D12WindowStartup &);
    // NEW HOST pre-device composition. Every other service is the actual
    // retained owner; menu is attached only after its real GPU producer.
    SourceApplicationMenuServices(SourceApplicationEntryServices &, FrontendState &, FrontendPerformanceCounter &,
                                  SceneTransitionServices &, SessionOuterFrameServices &, PolygonSubmissionSink &,
                                  LoadingFrameDevice &, SourceBootFilePlatform &, SourceApplicationWindowClock &,
                                  SourceApplicationSurfaceServices &, SourceColdInitializationContext &,
                                  SourceApplicationShutdownServices &, NewHostD3D12WindowStartup &);
    // One publication on the frontend's owner thread after actual GPU admission.
    // No source initializer, menu proxy, second owner or automatic reattachment.
    void attach_menu(SourceMenuFrameChildren &);
    SourceMenuFrameChildren &require_menu() const;
    // Validates attachment against the retained menu/character and the SAME
    // surface bridge used by the early native callback; does not clear input.
    void require_window_message_binding(SourceMenuNavigationBindingChildren &, SourceWindowMessageWords,
                                        SourceApplicationSurfaceServices &) const;
    // New host composition boundary for a WNDPROC: invokes the recovered
    // 44E6D0 against the exact menu object and its retained character byte.
    std::intptr_t dispatch_window_message(const SourceWindowMessage &, SourceMenuNavigationBindingChildren &,
                                          std::uint32_t &active, const std::uint32_t &mode,
                                          SourceWindowMessagePlatform &);
    void show_cursor(bool visible) override;
    std::uint32_t initialize_window_44e890(const SourceApplicationEntryArguments &) override;
    void invoke(SourceApplicationCall) override;
    SourceApplicationEntryResult run(const SourceApplicationEntryArguments &, const std::uint32_t &window_word_61c374,
                                     const std::uint32_t &window_word_61c364, SourceApplicationMessages &);
};

} // namespace sgn::retail
