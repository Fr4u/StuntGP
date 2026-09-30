#include "sgn/host_display_mode.hpp"
#include "sgn/renderer.hpp"
#include <stdexcept>
#include <system_error>

// NEW HOST (R24) display-mode controller; see the header for the policy.
namespace sgn::host_display
{
namespace
{
[[noreturn]] void throw_last_error(const char *action)
{
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), action);
}
void set_window_style(HWND window, LONG_PTR style)
{
    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(window, GWL_STYLE, style) == 0 && GetLastError() != ERROR_SUCCESS)
        throw_last_error("display mode: SetWindowLongPtr(GWL_STYLE)");
}
RECT monitor_rectangle(HWND window)
{
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
        throw_last_error("display mode: GetMonitorInfo");
    return monitor.rcMonitor;
}
constexpr std::uint64_t exclusive_retry_ms = 250; // after DXGI_ERROR_NOT_CURRENTLY_AVAILABLE
} // namespace

std::string_view name(DisplayMode mode) noexcept
{
    switch (mode)
    {
    case DisplayMode::windowed:
        return "windowed";
    case DisplayMode::borderless:
        return "borderless";
    case DisplayMode::exclusive:
        return "exclusive";
    }
    return "windowed";
}

void Controller::attach(HWND window)
{
    if (window_)
        throw std::logic_error("display-mode controller is already attached to a window");
    if (!window)
        throw std::invalid_argument("display-mode controller requires a window");
    // Revision before the snapshot (host_graphics contract): a set() racing in
    // between is seen again by the next poll, never skipped.
    revision_seen_ = host_graphics::revision();
    desired_ = host_graphics::current().display_mode;
    last_fullscreen_ = desired_ == DisplayMode::windowed ? DisplayMode::borderless : desired_;
    window_ = window;
    style_ = Style::windowed;
    placement_valid_ = refit_pending_ = exclusive_ = false;
    lost_renderer_ = nullptr;
    exclusive_retry_at_ = 0;
    if (desired_ != DisplayMode::windowed)
        enter_borderless_style(IsWindowVisible(window_) != FALSE);
}

void Controller::detach() noexcept
{
    window_ = nullptr;
    exclusive_ = false;
    lost_renderer_ = nullptr;
}

void Controller::on_activate_app(bool active) noexcept
{
    if (active)
        exclusive_retry_at_ = 0; // reactivation: re-enter exclusive at the next poll
}

void Controller::on_display_change() noexcept
{
    refit_pending_ = true;
}

bool Controller::fullscreen_style() const noexcept
{
    return window_ && style_ == Style::borderless;
}

DisplayMode Controller::applied() const noexcept
{
    if (exclusive_)
        return DisplayMode::exclusive;
    return style_ == Style::borderless ? DisplayMode::borderless : DisplayMode::windowed;
}

void Controller::toggle()
{
    auto settings = host_graphics::current();
    if (settings.display_mode != DisplayMode::windowed)
    {
        last_fullscreen_ = settings.display_mode;
        settings.display_mode = DisplayMode::windowed;
    }
    else
    {
        settings.display_mode = last_fullscreen_;
    }
    host_graphics::set(settings); // session value; the menus decide what is saved
}

void Controller::request(DisplayMode mode)
{
    auto settings = host_graphics::current();
    settings.display_mode = mode;
    host_graphics::set(settings);
}

// Raymond Chen's placement-preserving switch ("How do I switch a window between
// normal and fullscreen?"): the framed placement is saved before the popup
// style is applied and restored with SetWindowPlacement on the way back.
void Controller::enter_borderless_style(bool visible)
{
    const auto monitor = monitor_rectangle(window_);
    if (style_ == Style::windowed)
    {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        if (!GetWindowPlacement(window_, &placement))
            throw_last_error("display mode: GetWindowPlacement");
        placement_ = placement;
        placement_valid_ = true;
    }
    const auto style = GetWindowLongPtrW(window_, GWL_STYLE);
    // WS_MAXIMIZE is dropped with the frame: a maximized window would be
    // re-fitted to the work area (below the taskbar) by the window manager.
    // The saved placement restores the maximized state in windowed mode.
    set_window_style(window_, (style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE)) | WS_POPUP);
    UINT flags = SWP_NOOWNERZORDER | SWP_FRAMECHANGED;
    if (!visible)
        flags |= SWP_NOACTIVATE; // before the first ShowWindow: stays hidden
    if (!SetWindowPos(window_, HWND_TOP, monitor.left, monitor.top, monitor.right - monitor.left,
                      monitor.bottom - monitor.top, flags))
        throw_last_error("display mode: borderless SetWindowPos");
    style_ = Style::borderless;
    refit_pending_ = false;
}

void Controller::refit_borderless()
{
    const auto monitor = monitor_rectangle(window_);
    RECT current{};
    if (!GetWindowRect(window_, &current))
        throw_last_error("display mode: GetWindowRect");
    if (!EqualRect(&current, &monitor) &&
        !SetWindowPos(window_, nullptr, monitor.left, monitor.top, monitor.right - monitor.left,
                      monitor.bottom - monitor.top, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE))
        throw_last_error("display mode: borderless refit");
    refit_pending_ = false;
}

void Controller::enter_windowed_style()
{
    const auto style = GetWindowLongPtrW(window_, GWL_STYLE);
    set_window_style(window_, (style & ~static_cast<LONG_PTR>(WS_POPUP)) | WS_OVERLAPPEDWINDOW);
    if (placement_valid_)
    {
        auto placement = placement_;
        placement.length = sizeof(placement);
        placement.flags = 0;
        if (placement.showCmd != SW_SHOWMAXIMIZED)
            placement.showCmd = SW_SHOWNORMAL;
        if (!SetWindowPlacement(window_, &placement))
            throw_last_error("display mode: SetWindowPlacement");
    }
    if (!SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
        throw_last_error("display mode: windowed frame change");
    style_ = Style::windowed;
    refit_pending_ = false;
}

// Leaves exclusive fullscreen whenever it is not wanted (other mode chosen,
// focus lost, minimized) and records a loss DXGI made by itself. Returns the
// actual state afterwards.
bool Controller::reconcile_exclusive(Renderer &renderer, bool foreground, bool iconic)
{
    const bool want = desired_ == DisplayMode::exclusive && foreground && !iconic;
    bool actual = (desired_ == DisplayMode::exclusive || exclusive_) && renderer.exclusive_fullscreen();
    if (actual && !want && renderer.leave_exclusive_fullscreen())
        actual = false;
    if (exclusive_ && !actual)
    {
        refit_pending_ = true; // DXGI restores its pre-fullscreen window rectangle
        // Alt-tab / focus loss while exclusive: the desktop mode is back, the
        // game steps aside like a fullscreen game and returns on activation.
        if (desired_ == DisplayMode::exclusive && !foreground && !iconic)
            (void)ShowWindow(window_, SW_SHOWMINNOACTIVE); // returns the previous visibility, not an error
    }
    exclusive_ = actual;
    return actual;
}

bool Controller::poll(Renderer *renderer, bool defer_device_loss)
{
    if (!window_)
        return false;
    const auto before = applied();
    const auto revision = host_graphics::revision();
    if (revision != revision_seen_)
    {
        revision_seen_ = revision; // revision before the snapshot, as in attach()
        const auto next = host_graphics::current().display_mode;
        if (next != desired_)
            exclusive_retry_at_ = 0;
        desired_ = next;
        if (desired_ != DisplayMode::windowed)
            last_fullscreen_ = desired_;
    }
    // A renderer whose device was lost belongs to its recovery owner until it
    // is healthy again or replaced (a replacement starts windowed).
    if (lost_renderer_ && (renderer != lost_renderer_ || SUCCEEDED(renderer->host_state().device_status)))
        lost_renderer_ = nullptr;
    Renderer *const usable =
        renderer && renderer != lost_renderer_ && renderer->exclusive_fullscreen_supported() ? renderer : nullptr;
    const bool iconic = IsIconic(window_) != FALSE;
    const bool foreground = GetForegroundWindow() == window_;
    const auto guarded = [&](auto &&operation) {
        try
        {
            operation();
        }
        catch (const GraphicsError &error)
        {
            if (!defer_device_loss || !error.device_lost())
                throw;
            lost_renderer_ = usable; // the recovery owner recreates it; its destructor leaves fullscreen
            exclusive_ = false;
        }
    };
    // 1. Leave exclusive fullscreen before any style change.
    if (usable)
        guarded([&] { (void)reconcile_exclusive(*usable, foreground, iconic); });
    else if (exclusive_)
    {
        exclusive_ = false;
        refit_pending_ = true;
    } // renderer released: its destructor left fullscreen
    // 2. Window style. Never while minimized, never under DXGI's exclusive ownership.
    if (!iconic && !exclusive_)
    {
        const auto target = desired_ == DisplayMode::windowed ? Style::windowed : Style::borderless;
        if (target != style_)
        {
            if (target == Style::windowed)
                enter_windowed_style();
            else
                enter_borderless_style(IsWindowVisible(window_) != FALSE);
        }
        else if (style_ == Style::borderless && refit_pending_)
        {
            refit_borderless();
        }
    }
    // 3. Enter exclusive fullscreen from the borderless window.
    const auto now = GetTickCount64();
    if (usable && usable != lost_renderer_ && desired_ == DisplayMode::exclusive && !exclusive_ && foreground &&
        !iconic && style_ == Style::borderless && now >= exclusive_retry_at_)
    {
        guarded([&] {
            if (usable->enter_exclusive_fullscreen())
            {
                exclusive_ = true;
                exclusive_retry_at_ = 0;
            }
            else
            {
                exclusive_retry_at_ = now + exclusive_retry_ms; // DXGI: "try again later"
            }
        });
    }
    return applied() != before;
}
} // namespace sgn::host_display
