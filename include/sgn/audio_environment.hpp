#pragma once
#include "sgn/retail_audio_driver.hpp"
#include "sgn/vehicle_math.hpp"

namespace sgn::retail
{
using AudioPropertyGuid = std::array<std::uint8_t, 16>;
inline constexpr AudioPropertyGuid audio_property_interface{0x30, 0xac, 0xef, 0x31, 0x5c, 0x51, 0xd0, 0x11,
                                                            0xa9, 0xaa, 0,    0xaa, 0,    0x61, 0xbe, 0x93}; // 46FED0
inline constexpr AudioPropertyGuid audio_environment_property_set{0xa8, 0xa6, 0x06, 0x03, 0x24, 0xb2, 0xd2, 0x11, 0x99,
                                                                  0xe5, 0,    0,    0xe8, 0xd8, 0xc7, 0x22}; // 470500
struct AudioPropertyId
{
    std::uint64_t value{};
    bool operator==(const AudioPropertyId &) const = default;
};
struct AudioEnvironmentDevice
{
    virtual ~AudioEnvironmentDevice() = default;
    virtual AudioResult query_audio_property(AudioSpatialId, const AudioPropertyGuid &, AudioPropertyId &) = 0;
    virtual AudioResult audio_property_support(AudioPropertyId, const AudioPropertyGuid &, std::uint32_t,
                                               std::uint32_t &) = 0;
    virtual AudioResult set_audio_property(AudioPropertyId, const AudioPropertyGuid &, std::uint32_t,
                                           std::span<const std::uint8_t> instance,
                                           std::span<const std::uint8_t> data) = 0;
};
struct AudioEnvironmentRegion
{ // 483F98, source stride72, opaque lanes retained
    std::uint32_t type{}, opaque{};
    VehicleVector position{}, scale{}, v{}, n{};
};
static_assert(sizeof(AudioEnvironmentRegion) == 72);
struct AudioEnvironmentState
{
    std::array<AudioEnvironmentRegion, 32> regions{};
    std::uint32_t current_region{};             // 484898: typed index instead of guest pointer
    std::uint32_t count{};                      // 61B63C
    std::array<std::uint32_t, 14> properties{}; // 62E0B8..62E0EC
    std::uint32_t supported_properties{};       // 62F5C0
};
struct AudioEnvironmentWorld
{
    AudioEnvironmentState &state;
    const AudioDriverState &driver;
    const std::uint32_t &spatial_enabled;
    AudioEnvironmentDevice &device;
};
AudioEnvironmentState initial_audio_environment_state();                                 // literal PE state
bool audio_environment_contains(const AudioEnvironmentRegion &, Vec3);                   // 4278A0, full matrix/planes
bool audio_environment_support(AudioEnvironmentWorld &, AudioPropertyId, std::uint32_t); // 460700
bool set_audio_environment(AudioEnvironmentWorld &, std::uint32_t type);                 // 460AF0 incl460A80
void initialize_audio_environments(AudioEnvironmentWorld &);                             // 4460E0
void update_audio_environment(AudioEnvironmentWorld &, Vec3 camera_position);            // 446110
void load_audio_environments(std::span<const std::uint8_t>, AudioEnvironmentWorld &);    // 446700 incl4460E0
} // namespace sgn::retail
