#include "sgn/source_application_window_mode.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_profiler.hpp"
#include <Windows.h>

namespace sgn::retail
{
void SourceApplicationWindowModeServices::recreate_surface_422f30()
{
    auto world = display_mode_world();
    if (&world.viewport != &frontend_.world().viewport)
        throw FormatError("422F30 viewport belongs to another application world");
    auto &invocation = display_mode_invocation();
    (void)source_set_display_mode_422f30(world, invocation, 640, 480, 8);
}

void SourceApplicationWindowModeServices::window_mode_44e4b0(std::uint32_t mode)
{
    frontend_.shared_platform()->require_owner_thread();
    if (mode == 1)
    {
        if (mode_61c364_ != 0)
            return;
        auto *surface = surface_61c368(); // captured before publishing the flag
        mode_61c364_ = 1;
        sgn::host_profile::add("host.window_minimized", 0); // NEW HOST (R35) diagnostics only
        if (!surface)
            throw FormatError("44E4DE requires the source 61C368 surface");
        std::array<std::optional<std::uint32_t>, 4> caps;
        (void)surface->capabilities_slot38(caps); // HRESULT is ignored in source
        if (!caps[0])
            throw FormatError("44E4E1 reads an unwritten surface capability word");
        if ((*caps[0] & 0x200000u) != 0)
            recreate_surface_422f30();
        device_slot28_61c380();
        (void)DrawMenuBar(reinterpret_cast<HWND>(window_handle_61c398()));
        // Read the window again: DrawMenuBar may cause reentrant processing.
        (void)RedrawWindow(reinterpret_cast<HWND>(window_handle_61c398()), nullptr, nullptr, 0x400u);
        frontend_.world().outer.source_word_612994 = 0; // full44E490
        return;
    }
    // Every value other than1 takes this branch, including2 and0xFFFFFFFF.
    mode_61c364_ = 0;
    sgn::host_profile::add("host.window_restored", 0); // NEW HOST (R35) diagnostics only
    source_application_restore_44ea10(*this);
    // Restore may have re-entered mode1. Source does not clear the flag again.
    initialize_frontend_lookup_tables(frontend_.source);
    auto &fade = frontend_.world().light_fade;
    fade.last_gamma = 0.0f; // actual6244F4 owner
    set_light_gamma(fade, 1.0f, gamma_device_5da1dc());
}
} // namespace sgn::retail
