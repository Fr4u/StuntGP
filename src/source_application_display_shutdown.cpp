#include "sgn/source_application_display_shutdown.hpp"

namespace sgn::retail
{
namespace
{
void release_word(SourceDisplayReleaseReceiver *&word)
{
    auto *receiver = word;
    if (receiver)
    {
        (void)receiver->release_slot08();
        word = nullptr; // Source overwrites a reentrant replacement after Release.
    }
}
} // namespace
void release_source_display_interfaces_4228e0(SourceDisplayShutdownWords words)
{
    release_word(words.draw_61c378);   // 4228A0, ECX = &61C378
    release_word(words.device_61c380); // 4228C0, fresh read after first Release
    words.selected_61c358 = nullptr;
    words.primary_61c368 = nullptr;
    words.back_61c3a0 = nullptr;
}
void destroy_source_window_422930(std::uintptr_t *word, SourceDisplayWindowDestruction &windows)
{
    if (word)
    { // Source checks the slot address, not the HWND value.
        windows.destroy_window(*word);
        *word = 0;
    }
}
void release_source_display_422a60(SourceDisplayShutdownWords words, SourceApplicationSurfaceRegistry registry,
                                   SourceDisplayWindowDestruction &windows)
{
    if (!words.draw_61c378)
        return;
    release_source_surfaces_422a20(registry);
    release_source_display_interfaces_4228e0(words);
    destroy_source_window_422930(&words.window_61c398, windows);
}
void SourceApplicationDisplayShutdown::release_display_422a60()
{
    release_source_display_422a60(words_, registry_, windows_);
}
} // namespace sgn::retail
