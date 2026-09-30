#pragma once
#include "sgn/source_application_entry.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_application_draw_start_win32.hpp"
#include "sgn/source_application_palette_win32.hpp"
#include "sgn/source_application_surface_registry.hpp"
#include "sgn/source_application_display_mode.hpp"

namespace sgn::retail
{
struct RaceInitializationState;
struct SourceWindowStartupWords
{
    std::uintptr_t &instance_61c38c;
    std::uintptr_t &window_61c398;
    IDirectDraw *&draw_61c378;
    IDirectDraw4 *&device_61c380;
    IDirectDrawPalette *&palette_61c384;
    IDirectDrawSurface4 *&primary_61c368;
    IDirectDrawSurface4 *&back_61c3a0;
};
// Required bridge captures the SAME61C380 and destination words used above.
// It must resolve native surface identities through their retained wrappers.
struct SourceWindowSurfaceStartup
{
    virtual ~SourceWindowSurfaceStartup() = default;
    virtual SourceDisplayModeWorld display_world() = 0;
    virtual SourceDisplayModeInvocation &capture_display_invocation() = 0;
};
// Borrowed storage; caller retains all native slots and callback lifetimes.
struct SourceWindowStartupContext
{
    SourceWindowStartupWords words;
    WNDPROC procedure;
    SourceDrawStartupFailure &failure;
    SourceApplicationSurfaceRegistry registry;
    SourceWindowSurfaceStartup &surfaces;
    RaceInitializationState &keyboard;
    std::span<const std::uint8_t, 1024> palette_input;
    std::span<PALETTEENTRY, 256> palette_entries;
};
std::uint32_t initialize_source_window_44e890(const SourceApplicationEntryArguments &, SourceWindowStartupWords,
                                              SourceWindowStartupLiterals, WNDPROC, SourceDrawStartupFailure &,
                                              SourceApplicationSurfaceRegistry, SourceWindowSurfaceStartup &,
                                              RaceInitializationState &,
                                              std::span<const std::uint8_t, 1024> palette_input,
                                              std::span<PALETTEENTRY, 256> palette_entries);
} // namespace sgn::retail
