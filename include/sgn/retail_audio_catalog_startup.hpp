#pragma once
#include "sgn/retail_audio_startup.hpp"
#include "sgn/retail_audio_catalog.hpp"

namespace sgn::retail
{
struct AudioDeviceId
{
    std::uint64_t value{};
    bool operator==(const AudioDeviceId &) const = default;
};
struct AudioBufferRequest
{
    std::uint32_t size{}, flags{}, bytes{}, reserved{};
    std::optional<AudioWaveFormat> format;
};
struct AudioSampleLoadFields
{
    std::uint32_t ready{}, bytes{}, bits{}, channels{};
}; // sample+C/10/18/1C
struct AudioSampleLoadStorage
{
    std::array<AudioSampleLoadFields, 128> samples{};
};
struct AudioBufferMemory
{
    std::span<std::uint8_t> first, second;
};
struct AudioCreationDevice
{
    virtual ~AudioCreationDevice() = default;
    virtual AudioResult create_buffer(AudioDeviceId, const AudioBufferRequest &, AudioBufferId &) = 0;
    virtual AudioResult query_spatial_buffer(AudioBufferId, AudioSpatialId &) = 0;
    virtual AudioResult lock_buffer(AudioBufferId, std::uint32_t offset, std::uint32_t bytes, AudioBufferMemory &,
                                    std::uint32_t flags) = 0;
    virtual AudioResult unlock_buffer(AudioBufferId, std::span<std::uint8_t> first, std::span<std::uint8_t> second) = 0;
    virtual AudioResult capabilities(AudioDeviceId, std::span<std::uint32_t, 24>) = 0;
    virtual AudioResult primary_format(AudioPrimaryId, AudioWaveFormat &, std::uint32_t bytes,
                                       std::uint32_t &written) = 0;
    virtual AudioResult set_primary_format(AudioPrimaryId, const AudioWaveFormat &) = 0;
};
bool create_audio_sample_buffer(AudioSample &, const AudioSampleLoadFields &, AudioDeviceId, AudioCreationDevice &,
                                bool spatial, std::uint32_t flags); // 461B70/461C40
// Source canonical WAV44 layout. A missing asset returns false before mutation.
// Malformed/out-of-bounds layouts are rejected by the modern file boundary.
// Borrow the actual device/spatial owners: 461E92/461EAD and 461F86 reload
// their values at each source use, including after earlier external calls.
bool load_audio_sample(AudioSample &, AudioSampleLoadFields &, const AudioDeviceId &, AudioCreationDevice &,
                       const AssetStore &, std::string_view resource, const std::uint32_t &spatial_enabled,
                       std::uint32_t flags); // 461E00
bool load_audio_samples(AudioDriverState &, AudioSampleLoadStorage &, std::array<std::uint32_t, 72> &descriptor_types,
                        const AudioDeviceId &, AudioCreationDevice &, const AssetStore &,
                        const std::uint32_t &spatial_enabled); // 461F50
bool initialize_audio_primary(AudioDeviceId, AudioPrimaryId &, AudioCreationDevice &, std::uint32_t requested_rate,
                              std::uint32_t &spatial_capability, std::uint32_t &spatial_enabled); // 461CF0
} // namespace sgn::retail
