#include "sgn/source_application_menu_services.hpp"
#include "sgn/source_menu_navigation_binding.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_music_installed.hpp"
#include "sgn/source_application_window_start_d3d12.hpp"
#include <Windows.h>

namespace sgn::retail
{

extern "C" float source_application_frame_scale_x87(std::uint32_t milliseconds);

SourceApplicationMenuServices::SourceApplicationMenuServices(
    SourceApplicationEntryServices &remaining, FrontendState &frontend, FrontendPerformanceCounter &counter,
    SourceMenuFrameChildren &menu, SceneTransitionServices &transitions, SessionOuterFrameServices &frames,
    PolygonSubmissionSink &sink, LoadingFrameDevice &device, SourceBootFilePlatform &boot_files,
    SourceApplicationWindowClock &window_clock, SourceApplicationSurfaceServices &surfaces,
    SourceColdInitializationContext &cold, SourceApplicationShutdownServices &shutdown,
    SourceWindowStartupContext &window_startup)
    : remaining_(remaining), frontend_(frontend), counter_(counter), menu_(&menu), transitions_(transitions),
      frames_(frames), sink_(sink), device_(device), boot_files_(boot_files), window_clock_(window_clock),
      surfaces_(surfaces), cold_(cold), shutdown_(shutdown), window_startup_(&window_startup)
{
}

SourceApplicationMenuServices::SourceApplicationMenuServices(
    SourceApplicationEntryServices &remaining, FrontendState &frontend, FrontendPerformanceCounter &counter,
    SourceMenuFrameChildren &menu, SceneTransitionServices &transitions, SessionOuterFrameServices &frames,
    PolygonSubmissionSink &sink, LoadingFrameDevice &device, SourceBootFilePlatform &boot_files,
    SourceApplicationWindowClock &window_clock, SourceApplicationSurfaceServices &surfaces,
    SourceColdInitializationContext &cold, SourceApplicationShutdownServices &shutdown,
    NewHostD3D12WindowStartup &window_startup)
    : remaining_(remaining), frontend_(frontend), counter_(counter), menu_(&menu), transitions_(transitions),
      frames_(frames), sink_(sink), device_(device), boot_files_(boot_files), window_clock_(window_clock),
      surfaces_(surfaces), cold_(cold), shutdown_(shutdown), d3d12_window_startup_(&window_startup)
{
}

SourceApplicationMenuServices::SourceApplicationMenuServices(
    SourceApplicationEntryServices &remaining, FrontendState &frontend, FrontendPerformanceCounter &counter,
    SceneTransitionServices &transitions, SessionOuterFrameServices &frames, PolygonSubmissionSink &sink,
    LoadingFrameDevice &device, SourceBootFilePlatform &boot_files, SourceApplicationWindowClock &window_clock,
    SourceApplicationSurfaceServices &surfaces, SourceColdInitializationContext &cold,
    SourceApplicationShutdownServices &shutdown, NewHostD3D12WindowStartup &window_startup)
    : remaining_(remaining), frontend_(frontend), counter_(counter), transitions_(transitions), frames_(frames),
      sink_(sink), device_(device), boot_files_(boot_files), window_clock_(window_clock), surfaces_(surfaces),
      cold_(cold), shutdown_(shutdown), d3d12_window_startup_(&window_startup)
{
}

void SourceApplicationMenuServices::attach_menu(SourceMenuFrameChildren &menu)
{
    const auto platform = frontend_.shared_platform();
    platform->require_owner_thread();
    if (menu_)
        throw FormatError("Application menu is already attached; replacement is not permitted");
    if (!d3d12_window_startup_)
        throw FormatError("Late menu attachment requires NEW HOST D3D12 startup");
    if (&frontend_.world() != &platform->state() || &cold_.frontend != &frontend_ || &cold_.counter != &counter_ ||
        &cold_.files != &boot_files_ || &cold_.scene != &transitions_)
        throw FormatError("Late menu attachment borrows another application's owners");
    d3d12_window_startup_->require_gpu_admitted();
    menu_ = &menu; // Publish only after every admission check succeeds.
}

SourceMenuFrameChildren &SourceApplicationMenuServices::require_menu() const
{
    frontend_.shared_platform()->require_owner_thread();
    if (!menu_)
        throw FormatError("Application menu has not been attached after device production");
    return *menu_;
}

void SourceApplicationMenuServices::require_window_message_binding(SourceMenuNavigationBindingChildren &navigation,
                                                                   SourceWindowMessageWords words,
                                                                   SourceApplicationSurfaceServices &surfaces) const
{
    frontend_.shared_platform()->require_owner_thread();
    if (static_cast<SourceMenuFrameChildren *>(&navigation) != &require_menu() || &surfaces != &surfaces_)
        throw FormatError("Window input belongs to another menu or application surface composition");
    if (d3d12_window_startup_)
        d3d12_window_startup_->require_window_words(words);
    const auto retained = navigation.window_message_words(words.active_61c374, words.mode_61c364);
    if (&retained.character_62ddfc != &words.character_62ddfc)
        throw FormatError("Window input must retain the navigation character byte");
}
std::intptr_t SourceApplicationMenuServices::dispatch_window_message(const SourceWindowMessage &message,
                                                                     SourceMenuNavigationBindingChildren &navigation,
                                                                     std::uint32_t &active, const std::uint32_t &mode,
                                                                     SourceWindowMessagePlatform &platform)
{
    const auto words = navigation.window_message_words(active, mode);
    require_window_message_binding(navigation, words, surfaces_);
    return source_window_proc_44e6d0(message, words, surfaces_, platform);
}

void SourceApplicationMenuServices::show_cursor(bool visible)
{
    frontend_.shared_platform()->require_owner_thread();
    (void)ShowCursor(visible ? TRUE : FALSE);
}

SourceApplicationEntryResult SourceApplicationMenuServices::run(const SourceApplicationEntryArguments &arguments,
                                                                const std::uint32_t &window_word_61c374,
                                                                const std::uint32_t &window_word_61c364,
                                                                SourceApplicationMessages &messages)
{
    frontend_.shared_platform()->require_owner_thread();
    if (d3d12_window_startup_)
        d3d12_window_startup_->require_entry_words(window_word_61c374, window_word_61c364);
    auto &state = frontend_.world();
    SourceApplicationEntryWorld world{state.boot_config,  state.outer,
                                      state.scene_mode,   state.options.selection_phase,
                                      window_word_61c374, window_word_61c364};
    return run_source_application_44ea60(world, arguments, *this, messages);
}

std::uint32_t SourceApplicationMenuServices::initialize_window_44e890(const SourceApplicationEntryArguments &arguments)
{
    const auto platform = frontend_.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw FormatError("Source application startup cannot install on a preview platform");
    if (d3d12_window_startup_)
    {
        // NEW HOST window/input adoption only; GPU is produced later at42FD80.
        // No44E890 CreateWindow/DirectDraw path and no source surface flags.
        d3d12_window_startup_->prepare(arguments, frontend_, cold_, surfaces_);
        install_source_music_windows(platform);
        // Window-adoption continuation token (NOT GPU readiness), NOT
        // an observed44E890 return or a published source readiness DWORD.
        return 1u;
    }
    auto &startup = *window_startup_;
    if (&startup.keyboard != &frontend_.world().startup)
        throw FormatError("44E890 keyboard belongs to another application world");
    const auto application_literals = platform->source_application_literals();
    const auto result = initialize_source_window_44e890(
        arguments, startup.words, application_literals.window_startup_literals(), startup.procedure, startup.failure,
        startup.registry, startup.surfaces, startup.keyboard, startup.palette_input, startup.palette_entries);
    if (result != 0)
    {
        // New host composition boundary, not an additional recovered44E890
        // instruction. Prepare one runtime after the real window succeeds,
        // before442120 can reach scene children that request source music.
        // This registers a message-only dispatcher; it starts no music timer.
        install_source_music_windows(platform);
    }
    return result;
}

void SourceApplicationMenuServices::invoke(SourceApplicationCall operation)
{
    if (operation == SourceApplicationCall::shutdown_442180)
    {
        shutdown_source_application_442180(frontend_, shutdown_);
        return;
    }
    if (operation == SourceApplicationCall::initialize_scene_442120)
    {
        frontend_.shared_platform()->require_owner_thread();
        if (&cold_.frontend != &frontend_ || &cold_.counter != &counter_ || &cold_.files != &boot_files_ ||
            &cold_.scene != &transitions_)
            throw FormatError("442120 initialization borrows another application's services");
        initialize_source_cold_442120(cold_);
        return;
    }
    if (operation == SourceApplicationCall::initialize_442010)
    {
        initialize_platform_source_version_442010(*frontend_.shared_platform());
        return;
    }
    if (operation == SourceApplicationCall::idle_44e9d0)
    {
        frontend_.shared_platform()->require_owner_thread();
        if (d3d12_window_startup_)
        {
            // NEW HOST route before any legacy surface/resource traversal.
            d3d12_window_startup_->idle();
            return;
        }
        source_application_idle_44e9d0(surfaces_);
        return;
    }
    if (operation == SourceApplicationCall::finish_frame_44e6b0)
    {
        frontend_.shared_platform()->require_owner_thread();
        // 44E6B0 is a tail jump to 44E610. All arithmetic below is DWORD
        // arithmetic, including elapsed time across GetTickCount wraparound.
        auto &clock = window_clock_;
        clock.word_61c388 = clock.word_61c390;
        const std::uint32_t sampled = GetTickCount();
        const std::uint32_t frame_elapsed = sampled - clock.word_61c388;
        const std::uint32_t interval = sampled - clock.word_61c370;
        const std::uint32_t frames = clock.word_61c394 + 1u;
        clock.word_61c390 = sampled;
        clock.word_61c394 = frames;
        clock.word_61c36c = source_application_frame_scale_x87(frame_elapsed);
        if (interval > 1000u)
        {
            const std::uint32_t previous_frames = clock.word_61c35c;
            clock.word_61c35c = frames;
            clock.word_61c370 = sampled;
            const std::uint32_t numerator = frames * 1000u - previous_frames * 1000u;
            clock.word_61c39c = numerator / interval;
        }
        return;
    }
    if (operation == SourceApplicationCall::initialize_42d660)
    {
        initialize_platform_source_boot_config_42d660(*frontend_.shared_platform(), boot_files_);
        return;
    }
    if (operation == SourceApplicationCall::initialize_window_clock_44e5d0)
    {
        frontend_.shared_platform()->require_owner_thread();
        const auto sampled = GetTickCount(); // one actual read, 44E5D0
        window_clock_.word_61c37c = sampled;
        window_clock_.word_61c394 = 0;
        window_clock_.word_61c390 = sampled;
        window_clock_.word_61c370 = sampled;
        window_clock_.word_61c388 = sampled;
        return;
    }
    if (operation == SourceApplicationCall::sample_end_time_44e6c0)
    {
        frontend_.shared_platform()->require_owner_thread();
        window_clock_.word_61c390 = GetTickCount();
        return;
    }
    if (operation == SourceApplicationCall::update_menu_441f00)
    {
        update_source_menu_frame_441f00(frontend_, counter_, require_menu(), transitions_);
        return;
    }
    if (operation == SourceApplicationCall::draw_menu_441d70)
    {
        frontend_.shared_platform()->require_owner_thread();
        auto &world = frontend_.world();
        // The original draw gate precedes all resource accesses. In particular,
        // an update that entered a race must not borrow a stale menu frame.
        if (world.scene_mode != 2)
        {
            return;
        }
        auto &menu = require_menu(); // after the source scene gate, before resource borrows
        if (d3d12_window_startup_)
            d3d12_window_startup_->require_gpu_admitted();
        const auto words = frames_.device_words();
        if (&words.render_counter != &world.outer.render_counter ||
            &words.scene_active != &world.outer.device.scene_active ||
            &words.finalized != &world.outer.device.finalized)
        {
            throw FormatError("Application menu frame belongs to another world");
        }
        // Acquire the current material/viewport/scratch views now, never before
        // update or in this adapter's constructor. The borrow ends on return.
        auto frame = frames_.borrow_loading_frame();
        draw_source_menu_frame_441d70(frontend_, frame, sink_, device_, menu);
        return;
    }
    if (operation == SourceApplicationCall::draw_race_4427b0 && d3d12_window_startup_)
        d3d12_window_startup_->require_gpu_admitted();
    remaining_.invoke(operation);
}

} // namespace sgn::retail
