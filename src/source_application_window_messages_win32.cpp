#include "sgn/source_application_window_messages.hpp"
#include <Windows.h>
#include <stdexcept>

namespace sgn::retail
{
Win32SourceWindowMessagePlatform::Win32SourceWindowMessagePlatform() : thread_(GetCurrentThreadId()) {}
void Win32SourceWindowMessagePlatform::require_thread() const
{
    if (GetCurrentThreadId() != thread_)
        throw std::logic_error("Source window procedure called on another thread");
}
void Win32SourceWindowMessagePlatform::post_quit_zero()
{
    require_thread();
    PostQuitMessage(0);
}
bool Win32SourceWindowMessagePlatform::is_iconic(std::uintptr_t window)
{
    require_thread();
    return IsIconic(reinterpret_cast<HWND>(window)) != FALSE;
}
std::intptr_t Win32SourceWindowMessagePlatform::default_window_proc(const SourceWindowMessage &message)
{
    require_thread();
    return DefWindowProcA(reinterpret_cast<HWND>(message.window), message.id, static_cast<WPARAM>(message.wparam),
                          static_cast<LPARAM>(message.lparam));
}
} // namespace sgn::retail
