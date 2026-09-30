#pragma once
#include "sgn/menu_keyboard_acquisition.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

struct IDirectInputDevice2A;

namespace sgn::retail
{
// Actual external calls, identified by their original32-bit vtable offsets.
// Success must supply the consumed80-byte DIJOYSTATE representation. A failed
// GetDeviceState output is never consumed or synthesized by the source port.
class MenuDirectInputDevice
{
  public:
    virtual ~MenuDirectInputDevice() = default;
    virtual std::uint32_t poll_42454e() = 0;                              // virtual+64; result ignored
    virtual std::uint32_t get_state_42455a(std::span<std::byte, 80>) = 0; // virtual+24
    virtual std::uint32_t acquire_424564() = 0;                           // virtual+1C; result ignored
};

// Source576554 and normalized572CE8+(index*5A4) pointers. Construction means
// the PE's zero registry, not fabricated device discovery. Device objects and
// their configured/acquired COM interfaces must outlive synchronous calls.
struct MenuDirectInputRegistry
{
    std::uint32_t device_count{};
    std::array<MenuDirectInputDevice *, 10> devices{};
};

// Exact numeric leaf for4245C6..4245D4 /4245EE..4245FC under the caller's
// proved53-bit source precision (also exact for64-bit). Float46E690 is
// 0x3F828F5C =2139095/2097152. FILD32*that constant is exactly representable
// in53 bits for EVERY signed32 input.462860 FISTP64 truncates, then EAX is
// its low32 bits. No float narrowing, saturation or undefined int32 cast.
std::uint32_t direct_input_axis_word_4245c6(std::uint32_t raw_axis) noexcept;

// Complete424520..42467F, including guard return0, Poll/GetState/Acquire
// order, exact HRESULT==0 success, retained failure history and all stores.
// Valid source-caller slot domain0..9; table bounds/null violations after
// source guards are explicit host errors, not source-compatible fallbacks.
std::uint32_t acquire_direct_input_424520(MenuKeyboardRawState &, const MenuDirectInputRegistry &,
                                          std::uint32_t upstream_slot);

// Full4243F0 control flow with both actual children available. Unlike the
// previous keyboard-only domain wrapper, ordinary DirectInput return0 does
// NOT stop dispatch: source4243F0 ignores it and visits the next record.
void acquire_devices_4243f0(MenuKeyboardRawState &, RaceInitializationState &, MenuKeyboardAcquisition &,
                            const MenuDirectInputRegistry &);

// Source45F210 prefix only, through the existing4244C0. The remaining
// 45EF00/45F110/45EC20/45EE60 suffix and40DAD0 are separate integration work.
void acquire_publish_devices_prefix_45f210(MenuKeyboardRawState &, SessionMenuInputState &, RaceInitializationState &,
                                           std::uint32_t &second_pause_612958, MenuKeyboardAcquisition &,
                                           const MenuDirectInputRegistry &);

// Real Win32 receiver over a BORROWED IDirectInputDevice2A. The pinned source
// IID46F9E0 equals IID_IDirectInputDevice2A; native C++ calls choose the proper
// pointer-sized vtable layout. No hardcoded x86 offset is called on x64.
// Creation, configuration, acquisition and COM lifetime belong to the actual
// device registry; this adapter neither invents them nor substitutes keyboard.
class Win32MenuDirectInputDevice final : public MenuDirectInputDevice
{
  public:
    explicit Win32MenuDirectInputDevice(IDirectInputDevice2A &) noexcept;
    Win32MenuDirectInputDevice(const Win32MenuDirectInputDevice &) = delete;
    Win32MenuDirectInputDevice &operator=(const Win32MenuDirectInputDevice &) = delete;
    std::uint32_t poll_42454e() override;
    std::uint32_t get_state_42455a(std::span<std::byte, 80>) override;
    std::uint32_t acquire_424564() override;

  private:
    IDirectInputDevice2A &device_;
};
} // namespace sgn::retail
