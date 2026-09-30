#include "sgn/menu_keyboard_acquisition.hpp"
#include <Windows.h>

namespace sgn::retail
{
namespace
{
// Source45EC00..45EC0B: one real OS call into the actual owner's256 bytes.
// Do not repair, clear or replace its output according to the ignored BOOL.
void get_keyboard_state_45ec00(std::span<std::uint8_t, 256> state) noexcept
{
    (void)::GetKeyboardState(state.data());
    // NEW HOST (R24): Alt+Enter belongs to the host display-mode toggle
    // (host_display_mode). While Alt is held the Return key is not reported, so
    // the same keystroke cannot also confirm a menu row or act in the race.
    if ((state[VK_MENU] & 0x80u) != 0)
        state[VK_RETURN] = 0;
}
} // namespace
Win32MenuKeyboardAcquisition::Win32MenuKeyboardAcquisition(std::uintptr_t window) : window_(window), message_thread_(0)
{
    DWORD process{};
    const auto thread = ::GetWindowThreadProcessId(reinterpret_cast<HWND>(window_), &process);
    if (thread == 0 || process != ::GetCurrentProcessId() || thread != ::GetCurrentThreadId())
        throw FormatError("keyboard acquisition requires the local window message thread");
    message_thread_ = thread;
}
void Win32MenuKeyboardAcquisition::acquire_45ec00(std::span<std::uint8_t, 256> state)
{
    if (::GetCurrentThreadId() != message_thread_ ||
        ::GetWindowThreadProcessId(reinterpret_cast<HWND>(window_), nullptr) != message_thread_)
        throw FormatError("keyboard acquisition window or message thread changed");
    get_keyboard_state_45ec00(state);
}
} // namespace sgn::retail
