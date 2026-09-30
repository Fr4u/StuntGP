#pragma once
#include <cstdint>
#include <memory>

namespace sgn::retail
{
struct MenuKeyboardRawState;
struct MenuDirectInputRegistry;
class Win32MenuKeyboardAcquisition;
class Win32MenuDirectInputStartup;
struct InputWindowHandles
{
    std::uintptr_t instance{};
    std::uintptr_t window{};
};
// NEW construction view: actual adapters, no registry/readiness claim.
// Legal before4249B0; source operations retain their own phase checks.
struct WindowInputConstructionBorrow
{
    Win32MenuKeyboardAcquisition &keyboard;
    Win32MenuDirectInputStartup &direct_input;
};
// Invocation-only references; they do not retain the platform or extend its
// lifetime. Registry is always returned by this SAME startup after it entered
// the returned phase. The wrapper never constructs an empty fallback registry.
struct WindowInputBorrow
{
    Win32MenuKeyboardAcquisition &keyboard;
    Win32MenuDirectInputStartup &direct_input;
    const MenuDirectInputRegistry &registry;
};

// New host ownership, not an original source initializer. Construct only after
// a local top-level HWND exists, on its message thread, with its real HINSTANCE.
// Raw owner remains live; original HWND identity survives source destruction. The
// owner borrows raw and handles; it owns exactly one keyboard and one startup.
// No RaceSession dependency, world copy, source init, reset or polling in ctor.
class WindowInputOwner final
{
  public:
    WindowInputOwner(MenuKeyboardRawState &, InputWindowHandles);
    ~WindowInputOwner(); // residual host finalization only; NEVER424930
    WindowInputOwner(const WindowInputOwner &) = delete;
    WindowInputOwner &operator=(const WindowInputOwner &) = delete;
    WindowInputOwner(WindowInputOwner &&) = delete;
    WindowInputOwner &operator=(WindowInputOwner &&) = delete;

    void require_binding(const MenuKeyboardRawState &, InputWindowHandles) const;
    [[nodiscard]] WindowInputConstructionBorrow borrow_for_menu_construction(MenuKeyboardRawState &,
                                                                             InputWindowHandles);
    // Genuine caller events. Exact source return preserved; exceptions remain
    // exceptions. Startup owns its phase; this wrapper has no second flag.
    [[nodiscard]] std::uint32_t initialize_4249b0(MenuKeyboardRawState &, InputWindowHandles);
    // May follow DestroyWindow; same thread/raw/original handles still required.
    void shutdown_424930(MenuKeyboardRawState &, InputWindowHandles);
    [[nodiscard]] WindowInputBorrow borrow_initialized(MenuKeyboardRawState &, InputWindowHandles);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace sgn::retail
