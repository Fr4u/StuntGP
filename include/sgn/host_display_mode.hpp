#pragma once
#include "sgn/host_graphics_settings.hpp"
#include <cstdint>
#include <string_view>
#include <windows.h>

namespace sgn
{
class Renderer;
}

// NEW HOST (R24) display modes. Not recovered behaviour: the 2001 original
// switched DirectDraw display modes itself; this host presents the unchanged
// source frames in one application window and never changes a value the
// simulation or the source frontend reads.
//
// The mode is sgn::host_graphics::Settings::display_mode:
//  windowed   - resizable WS_OVERLAPPEDWINDOW at the saved placement (position,
//               size and maximized state survive every fullscreen round trip).
//  borderless - "fullscreen windowed": WS_POPUP exactly covering rcMonitor of
//               the window's monitor. A foreground window covering its monitor
//               is shown above the taskbar; alt-tab is a plain activation.
//  exclusive  - the borderless window plus DXGI exclusive fullscreen of the
//               flip-model swap chain at the monitor's native resolution and
//               highest refresh rate (Renderer::enter_exclusive_fullscreen),
//               held only while the window is foreground and not minimized. On
//               focus loss the swap chain returns to windowed and the window
//               minimizes (as games do); on reactivation exclusive fullscreen is
//               entered again. A renderer without ALLOW_MODE_SWITCH (the
//               non-game diagnostic paths) shows exclusive as borderless.
//
// Threading and reentrancy: one Controller per application window, used only
// on that window's thread. Window-message observers (on_*) only record; every
// style, placement and DXGI change happens in poll(), which the caller runs at
// a safe point: outside every renderer call, source frame and window callback
// (style changes and DXGI transitions send window messages synchronously).
namespace sgn::host_display
{
using DisplayMode = host_graphics::DisplayMode;

std::string_view name(DisplayMode) noexcept;

class Controller
{
  public:
    // Before the window is first shown: reads host_graphics (revision first,
    // then the snapshot) and installs that mode's style and extent without
    // showing or activating the window, so a fullscreen start never flashes a
    // framed window. Exclusive starts borderless; poll() enters the exclusive
    // mode once a swap chain exists and the window is foreground.
    void attach(HWND window);
    void detach() noexcept;
    // Message observers: record only (no style, placement or DXGI calls).
    void on_activate_app(bool active) noexcept;
    // WM_DISPLAYCHANGE, or WM_DPICHANGED while fullscreen_style(): the borderless
    // window is refitted to its monitor at the next poll().
    void on_display_change() noexcept;
    // True while the window has the fullscreen (popup) style. Callers must then
    // not apply WM_DPICHANGED suggested rectangles.
    bool fullscreen_style() const noexcept;
    // Alt+Enter: windowed <-> the last fullscreen mode, through
    // host_graphics::set (never saved here; saving belongs to the menus).
    void toggle();
    // Explicit request through host_graphics::set (e.g. Esc in the preview host).
    void request(DisplayMode);
    // Safe point, see above. renderer may be null (before the swap chain exists
    // or while it is recreated). defer_device_loss: a device-lost GraphicsError
    // from the renderer is left to the caller's existing recovery owner (the
    // renderer is not used again until it is healthy or replaced) instead of
    // being rethrown. Returns true when the applied mode changed.
    bool poll(Renderer *renderer, bool defer_device_loss);
    DisplayMode applied() const noexcept;
    DisplayMode desired() const noexcept
    {
        return desired_;
    }

  private:
    enum class Style : std::uint8_t
    {
        windowed,
        borderless
    };
    HWND window_{};
    DisplayMode desired_{DisplayMode::windowed};
    DisplayMode last_fullscreen_{DisplayMode::borderless};
    std::uint64_t revision_seen_{};
    Style style_{Style::windowed};
    WINDOWPLACEMENT placement_{};
    bool placement_valid_{};
    bool refit_pending_{};
    bool exclusive_{};                   // exclusive fullscreen reached by this controller
    const Renderer *lost_renderer_{};    // device lost; left to the recovery owner
    std::uint64_t exclusive_retry_at_{}; // GetTickCount64 value; 0 = now
    void enter_windowed_style();
    void enter_borderless_style(bool visible);
    void refit_borderless();
    bool reconcile_exclusive(Renderer &renderer, bool foreground, bool iconic);
};
} // namespace sgn::host_display
