#include "sgn/menu_directinput_acquisition.hpp"
#include <Windows.h>
#include <dinput.h>
#include <cstddef>

namespace sgn::retail
{
static_assert(sizeof(DIJOYSTATE) == 80);
static_assert(offsetof(DIJOYSTATE, lX) == 0 && offsetof(DIJOYSTATE, lY) == 4);
static_assert(offsetof(DIJOYSTATE, rgdwPOV) == 32 && offsetof(DIJOYSTATE, rgbButtons) == 48);
static_assert(sizeof(HRESULT) == sizeof(std::uint32_t));
Win32MenuDirectInputDevice::Win32MenuDirectInputDevice(IDirectInputDevice2A &device) noexcept : device_(device) {}
std::uint32_t Win32MenuDirectInputDevice::poll_42454e()
{
    return static_cast<std::uint32_t>(device_.Poll());
}
std::uint32_t Win32MenuDirectInputDevice::get_state_42455a(std::span<std::byte, 80> packet)
{
    return static_cast<std::uint32_t>(device_.GetDeviceState(80, packet.data()));
}
std::uint32_t Win32MenuDirectInputDevice::acquire_424564()
{
    return static_cast<std::uint32_t>(device_.Acquire());
}
} // namespace sgn::retail
