#pragma once
#include <cstdint>
#include <span>

namespace sgn::retail
{
// Required external receivers. HRESULT bits are literal; no default state or
// device/driver implementation is supplied by the recovered setup routines.
struct DeviceSetupDevice
{
    virtual ~DeviceSetupDevice() = default;
    virtual std::uint32_t begin_scene() = 0;                                                        // slot24
    virtual std::uint32_t end_scene() = 0;                                                          // slot28
    virtual std::uint32_t set_render_state(std::uint32_t, std::uint32_t) = 0;                       // slot58
    virtual std::uint32_t set_light_state(std::uint32_t, std::uint32_t) = 0;                        // slot60
    virtual std::uint32_t set_texture_stage_state(std::uint32_t, std::uint32_t, std::uint32_t) = 0; // slotA0
};
struct DeviceSetupSurface
{
    virtual ~DeviceSetupSurface() = default;
    // The61C358 surface receiver, not a material texture. Key storage is a
    // synchronous borrow and cannot be retained after this call returns.
    virtual std::uint32_t set_color_key(std::uint32_t, std::span<const std::uint32_t, 2>) = 0; // slot74
};
// Live views of existing descriptor/global owners; no mirrored capabilities
// or cached feature flags. Receiver pointer storage must outlive the call.
struct DeviceSetupCapabilities
{
    const std::uint32_t &field_e4;
    const std::uint32_t &field_e8;
    const std::uint32_t &field_f0;
    const std::uint16_t &texture_units_160;
    const std::uint16_t &blend_stages_162;
    DeviceSetupDevice *const &initialization_device_164;
};
struct DeviceSetupGlobals
{
    std::uint32_t &flag_592068;
    std::uint32_t &flag_59206c;
    std::uint32_t &flag_592070;
    std::uint32_t &flag_592074;
    std::uint32_t &flag_592078;
    std::uint32_t &flag_6244f0;
};
struct DeviceSetupWorld
{
    DeviceSetupCapabilities capabilities;
    DeviceSetupGlobals globals;
    DeviceSetupDevice *const &global_device_5da1c4;
    DeviceSetupSurface *const &surface_61c358; // null is the source absent-surface branch
};
void configure_device_filter_42f9c0(DeviceSetupWorld);
void configure_device_color_key_42f930(DeviceSetupWorld);
void configure_device_texture_42fa10(DeviceSetupWorld);
void device_setup_noop_42fa00() noexcept;
void configure_device_multitexture_42fa40(DeviceSetupWorld) noexcept;
// Full42FA80. Initialization receiver is cached once; all global-device reads
// remain live. Nonzero Begin/End HRESULT returns0; ignored setter HRESULTs do
// not prevent later source-global writes. Missing required host receiver throws.
std::uint32_t initialize_retail_device(DeviceSetupWorld);
} // namespace sgn::retail
