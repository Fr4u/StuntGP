#include "sgn/source_application_window_initialize.hpp"
#include "sgn/source_application_window_start_win32.hpp"

namespace sgn::retail
{
std::uint32_t initialize_source_window_44e890(const SourceApplicationEntryArguments &args,
                                              SourceWindowStartupWords words, SourceWindowStartupLiterals literals,
                                              WNDPROC procedure, SourceDrawStartupFailure &failure,
                                              SourceApplicationSurfaceRegistry registry,
                                              SourceWindowSurfaceStartup &surfaces, RaceInitializationState &keyboard,
                                              std::span<const std::uint8_t, 1024> palette_input,
                                              std::span<PALETTEENTRY, 256> palette_entries)
{
    // Actual instruction order, opposite to the decompiler's first two lines.
    words.instance_61c38c = args.instance; // 44E89E
    words.palette_61c384 = nullptr;        // 44E8A4; not a Release
    if (args.previous_instance == 0)
    {
        const auto atom = register_source_window_class_44e8ac(reinterpret_cast<HINSTANCE>(args.instance), procedure,
                                                              literals.menu_486474, literals.registered_class_48647c);
        if (atom == 0)
            return 0;
    }
    const auto window =
        create_source_window_422700(args.instance, literals.created_class_486494, literals.title_486488);
    words.window_61c398 = window; // 44E91A, even when zero
    if (window == 0)
        return 0;
    (void)create_source_draw_4227e0(words.draw_61c378, nullptr, window, failure);
    // Reload HWND after creation/cooperative calls, as44E938 does.
    (void)query_source_draw4_422820(words.draw_61c378, words.device_61c380, words.window_61c398, failure);
    // Parent never reads these locals. Initial host values are not defaults
    // published to the game; the source query stores only reached outputs.
    std::uint32_t video_total{}, texture_total{}, video_free{};
    (void)query_source_video_memory_422950(words.device_61c380, video_total, texture_total, video_free);
    clear_source_surface_registry_4229e0(registry);
    auto &invocation = surfaces.capture_display_invocation();
    (void)source_set_display_mode_422f30(surfaces.display_world(), invocation, 640, 480, 8);
    acquire_source_keyboard_44e870(keyboard);
    const auto result = initialize_source_palette_4230b0(words.draw_61c378, words.primary_61c368, words.back_61c3a0,
                                                         words.palette_61c384, palette_input, palette_entries);
    // show_command and command_line are popped but never read in this body.
    return result == 0 ? 1u : 0u;
}
} // namespace sgn::retail
