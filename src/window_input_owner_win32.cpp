#include "sgn/window_input_owner.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include <Windows.h>
#include <exception>

namespace sgn::retail
{
namespace
{
struct Identity
{
    MenuKeyboardRawState *raw;
    InputWindowHandles handles;
    DWORD thread;
};
Identity validate_creation(MenuKeyboardRawState &raw, InputWindowHandles handles)
{
    const auto window = reinterpret_cast<HWND>(handles.window);
    DWORD process{};
    const auto thread = ::GetWindowThreadProcessId(window, &process);
    if (!handles.instance || !thread || process != ::GetCurrentProcessId() || thread != ::GetCurrentThreadId() ||
        ::GetAncestor(window, GA_ROOT) != window ||
        static_cast<std::uintptr_t>(::GetWindowLongPtrW(window, GWLP_HINSTANCE)) != handles.instance)
        throw FormatError("window input requires the actual local top-level HWND and HINSTANCE on its message thread");
    return {&raw, handles, thread};
}
} // namespace
struct WindowInputOwner::Impl
{
    const Identity identity; // validated BEFORE either native adapter constructs
    Win32MenuKeyboardAcquisition keyboard;
    Win32MenuDirectInputStartup direct_input;
    Impl(MenuKeyboardRawState &raw, InputWindowHandles handles)
        : identity(validate_creation(raw, handles)), keyboard(handles.window),
          direct_input(raw, handles.instance, handles.window)
    {
    }
    ~Impl()
    {
        // The HWND may already have been destroyed during exception unwind.
        // Finalization needs the owning thread, not a second source shutdown.
        // direct_input is destroyed first while borrowed raw storage is alive.
        if (::GetCurrentThreadId() != identity.thread)
            std::terminate();
    }
    // Shutdown follows source422A60 DestroyWindow. Identity/thread remain
    // mandatory; a live HWND is required only by init/acquisition/borrows.
    void require_shutdown_identity(const MenuKeyboardRawState &raw, InputWindowHandles handles) const
    {
        if (::GetCurrentThreadId() != identity.thread)
            throw FormatError("window input shutdown belongs to its creating message thread");
        if (&raw != identity.raw || handles.instance != identity.handles.instance ||
            handles.window != identity.handles.window)
            throw FormatError("window input shutdown binding belongs to another raw owner or original handles");
    }
    void require(const MenuKeyboardRawState &raw, InputWindowHandles handles) const
    {
        if (::GetCurrentThreadId() != identity.thread)
            throw FormatError("window input belongs to its creating message thread");
        if (&raw != identity.raw || handles.instance != identity.handles.instance ||
            handles.window != identity.handles.window)
            throw FormatError("window input binding belongs to another raw owner or native window");
        const auto window = reinterpret_cast<HWND>(handles.window);
        DWORD process{};
        if (::GetWindowThreadProcessId(window, &process) != identity.thread || process != ::GetCurrentProcessId() ||
            ::GetAncestor(window, GA_ROOT) != window ||
            static_cast<std::uintptr_t>(::GetWindowLongPtrW(window, GWLP_HINSTANCE)) != handles.instance)
            throw FormatError("window input HWND or HINSTANCE is no longer valid for this lifetime");
    }
};
WindowInputOwner::WindowInputOwner(MenuKeyboardRawState &raw, InputWindowHandles handles)
    : impl_(std::make_unique<Impl>(raw, handles))
{
}
WindowInputOwner::~WindowInputOwner() = default;
void WindowInputOwner::require_binding(const MenuKeyboardRawState &raw, InputWindowHandles handles) const
{
    impl_->require(raw, handles);
}
WindowInputConstructionBorrow WindowInputOwner::borrow_for_menu_construction(MenuKeyboardRawState &raw,
                                                                             InputWindowHandles handles)
{
    impl_->require(raw, handles);
    return {impl_->keyboard, impl_->direct_input};
}
std::uint32_t WindowInputOwner::initialize_4249b0(MenuKeyboardRawState &raw, InputWindowHandles handles)
{
    impl_->require(raw, handles);
    return impl_->direct_input.initialize_4249b0();
}
void WindowInputOwner::shutdown_424930(MenuKeyboardRawState &raw, InputWindowHandles handles)
{
    impl_->require_shutdown_identity(raw, handles);
    impl_->direct_input.shutdown_424930();
}
WindowInputBorrow WindowInputOwner::borrow_initialized(MenuKeyboardRawState &raw, InputWindowHandles handles)
{
    impl_->require(raw, handles);
    const auto &registry = impl_->direct_input.registry_for_acquisition();
    return {impl_->keyboard, impl_->direct_input, registry};
}
} // namespace sgn::retail
