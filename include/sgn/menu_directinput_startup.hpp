#pragma once
#include "sgn/menu_directinput_acquisition.hpp"
#include <array>
#include <cstdint>
#include <memory>

struct IDirectInputA;
struct IDirectInputEffect;
struct DIEFFECT;

namespace sgn::retail
{
// Relocated source records, NOT a byte-compatible x86 allocation. Each source
// effect is one pointer at +4+n*78, then 29 DWORDs at +8+n*78. Native pointers
// remain pointer-sized; no original address is dereferenced by the host.
struct MenuDirectInputEffectRecord
{
    IDirectInputEffect *effect{};
    std::array<std::uint32_t, 29> payload{};
};
struct MenuDirectInputNativeRecord
{
    IDirectInputDevice2A *device{};
    std::array<MenuDirectInputEffectRecord, 12> effects{};
};
struct MenuDirectInputStartupState
{
    std::array<MenuDirectInputNativeRecord, 10> records;
    IDirectInputA *root_576b68{};
    std::uint32_t cursor_576550{};
    std::array<std::array<std::uint32_t, 4>, 4> effect_guids_576558{};
    std::uint32_t flag_576b74{};
    // 576554 belongs ONLY to MenuDirectInputRegistry::device_count.
    // Upstream576618 and pointer576B6C belong ONLY to MenuKeyboardRawState.
};

// One source startup/shutdown lifetime on the window's message thread.
// Uses system dinput.dll!DirectInputCreateA(version500), actual callbacks and
// actual Device2A COM objects. Window and module instance are borrowed native
// handles, corresponding to61C398 and61C38C. Source shutdown may follow
// DestroyWindow; the original thread and raw owner must still outlive it.
class Win32MenuDirectInputStartup final
{
  public:
    Win32MenuDirectInputStartup(MenuKeyboardRawState &, std::uintptr_t instance, std::uintptr_t window);
    ~Win32MenuDirectInputStartup();
    Win32MenuDirectInputStartup(const Win32MenuDirectInputStartup &) = delete;
    Win32MenuDirectInputStartup &operator=(const Win32MenuDirectInputStartup &) = delete;
    Win32MenuDirectInputStartup(Win32MenuDirectInputStartup &&) = delete;
    Win32MenuDirectInputStartup &operator=(Win32MenuDirectInputStartup &&) = delete;

    // Exact source return0/1. Host errors throw separately, never become a
    // source return or a fabricated keyboard/DirectInput8 recovery path.
    [[nodiscard]] std::uint32_t initialize_4249b0();
    [[nodiscard]] const MenuDirectInputRegistry &registry_for_acquisition() const;
    [[nodiscard]] const MenuDirectInputStartupState &source_state() const noexcept;

    // Synchronous force-feedback borrow of THIS startup and THIS raw owner.
    // No ownership transfer, acquisition reset, second device bank or registry.
    // All calls require the initialized phase and original window/thread.
    [[nodiscard]] MenuDirectInputStartupState &borrow_force_feedback_state(MenuKeyboardRawState &);
    [[nodiscard]] std::uint32_t create_force_feedback_effect(MenuKeyboardRawState &, unsigned record, unsigned effect,
                                                             const std::array<std::uint32_t, 4> &guid,
                                                             const DIEFFECT &);
    [[nodiscard]] std::uint32_t set_force_feedback_parameters(MenuKeyboardRawState &, IDirectInputEffect &,
                                                              const DIEFFECT &, std::uint32_t flags);
    [[nodiscard]] std::uint32_t start_force_feedback(MenuKeyboardRawState &, IDirectInputEffect &,
                                                     std::uint32_t iterations, std::uint32_t flags);

    // Exact source Release/Unacquire calls and retained pointer/count words.
    // Source is NOT idempotent. Host rejects a repeated shutdown or restart.
    // Owner-thread/phase checks survive source422A60 destroying the HWND first.
    void shutdown_424930();

    // New host finalizer: releases references left over by source callbacks,
    // including retained basic and orphaned QI references. Destructor invokes
    // this only; it does NOT pretend the source shutdown ran. A normal caller
    // must call shutdown_424930 first. Calling this invalidates every borrowed
    // registry/COM view. No subsequent source operation is permitted.
    void finalize_host() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace sgn::retail
