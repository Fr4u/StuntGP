#include "sgn/source_application_window_messages.hpp"
#include "sgn/host_profiler.hpp"

namespace sgn::retail
{
std::intptr_t source_window_proc_44e6d0(const SourceWindowMessage &message, SourceWindowMessageWords words,
                                        SourceApplicationSurfaceServices &surfaces,
                                        SourceWindowMessagePlatform &platform)
{
    if (message.id <= 5u)
    {
        if (message.id == 2u)
        {
            platform.post_quit_zero();
        }
        else if (message.id == 3u || message.id == 5u)
        {
            if (platform.is_iconic(message.window))
                surfaces.window_mode_44e4b0(1u);
        }
    }
    else if (message.id == 0x1cu)
    {
        // Only the source DWORD store is narrowed; native default processing
        // receives the full original WPARAM and LPARAM on x64.
        const auto source_wparam = static_cast<std::uint32_t>(message.wparam);
        words.active_61c374 = source_wparam;
        // NEW HOST (R35) diagnostics only (frame trace: the source pauses while inactive); no source effect.
        sgn::host_profile::add(source_wparam != 0u ? "host.window_activated" : "host.window_deactivated", 0);
        if (source_wparam != 0u && words.mode_61c364 == 1u)
            surfaces.window_mode_44e4b0(0u);
    }
    else if (message.id == 0x102u && words.character_62ddfc == 0u)
    {
        words.character_62ddfc = static_cast<std::uint8_t>(message.wparam);
    }
    // Every original path calls DefWindowProcA and returns its EAX, despite
    // Ghidra's void prototype. WM_CHAR is neither consumed nor queued here.
    return platform.default_window_proc(message);
}
} // namespace sgn::retail
