#include "sgn/retail_audio_catalog_startup.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint32_t u32(std::span<const std::uint8_t> b, std::size_t at)
{
    std::uint32_t value;
    if (at > b.size() || b.size() - at < 4)
        throw FormatError("truncated source WAV");
    std::memcpy(&value, b.data() + at, 4);
    return value;
}
std::int32_t i16(std::span<const std::uint8_t> b, std::size_t at)
{
    std::int16_t value;
    if (at > b.size() || b.size() - at < 2)
        throw FormatError("truncated source WAV");
    std::memcpy(&value, b.data() + at, 2);
    return value;
}
AudioWaveFormat sample_format(const AudioSample &sample, const AudioSampleLoadFields &fields)
{
    AudioWaveFormat format;
    format.tag = 1;
    format.channels = static_cast<std::uint16_t>(fields.channels);
    format.bits_per_sample = static_cast<std::uint16_t>(fields.bits);
    format.samples_per_second = sample.frequency;
    format.block_align = static_cast<std::uint16_t>((fields.bits * fields.channels) >> 3);
    format.bytes_per_second = std::uint32_t(format.block_align) * sample.frequency;
    return format;
}
} // namespace
bool create_audio_sample_buffer(AudioSample &sample, const AudioSampleLoadFields &fields, AudioDeviceId device,
                                AudioCreationDevice &api, bool spatial, std::uint32_t flags)
{
    const AudioBufferRequest request{20, spatial ? (flags | 0xe0u) : 0xe2u, fields.bytes, 0,
                                     sample_format(sample, fields)};
    AudioBufferId created{};
    if (api.create_buffer(device, request, created) != 0)
        return false;
    if (spatial)
    {
        AudioSpatialId found{};
        if (api.query_spatial_buffer(created, found) != 0)
            return false;
        sample.buffer = created;
        sample.spatial = found;
    }
    else
        sample.buffer = created;
    return true;
}
bool load_audio_sample(AudioSample &sample, AudioSampleLoadFields &fields, const AudioDeviceId &device,
                       AudioCreationDevice &api, const AssetStore &assets, std::string_view resource,
                       const std::uint32_t &spatial_enabled, std::uint32_t flags)
{
    std::span<const std::uint8_t> bytes;
    try
    {
        bytes = assets.resource(resource);
    }
    catch (const FormatError &)
    {
        return false;
    }
    if (bytes.size() < 44 || u32(bytes, 0) != 0x46464952 || u32(bytes, 8) != 0x45564157 ||
        u32(bytes, 12) != 0x20746d66 || u32(bytes, 16) != 16 || u32(bytes, 36) != 0x61746164)
        throw FormatError("source audio loader requires canonical WAV44 layout");
    const auto sample_bytes = u32(bytes, 40);
    const auto divisor = std::bit_cast<std::uint32_t>(i16(bytes, 32)) << 3;
    if (!divisor || sample_bytes > bytes.size() - 44)
        throw FormatError("source audio sample division or payload bounds");
    sample.type = (flags & 0x10) ? 2u : 1u;
    sample.frequency = u32(bytes, 24);
    fields.bytes = sample_bytes;
    fields.bits = std::bit_cast<std::uint32_t>(i16(bytes, 34));
    fields.channels = fields.bits / divisor;
    sample.buffer = {};
    fields.ready = 1;
    if (!create_audio_sample_buffer(sample, fields, device, api, spatial_enabled == 1 && (flags & 0x10) != 0, flags))
        return false;
    const auto locked_buffer = sample.buffer; // 461ECC/461EDD: retained through Lock and Unlock
    const auto locked_bytes = fields.bytes;   // 461ED0/461EE9: retained through the payload copy
    AudioBufferMemory memory;
    if (api.lock_buffer(locked_buffer, 0, locked_bytes, memory, 2) != 0)
        return false;
    if (memory.first.size() < locked_bytes)
        throw std::out_of_range("source audio lock first region too small");
    // Host memory boundary at the source copy (461F0B). A creator may change
    // the count; retain its pre-Lock value without reading beyond the WAV owner.
    if (locked_bytes > bytes.size() - 44)
        throw std::out_of_range("source audio payload exceeds retained WAV storage");
    const auto first = memory.first.first(locked_bytes);
    std::copy_n(bytes.begin() + 44, locked_bytes, first.begin());
    if (api.unlock_buffer(locked_buffer, first, {}) != 0)
        return false;
    return true;
}
bool load_audio_samples(AudioDriverState &driver, AudioSampleLoadStorage &storage, std::array<std::uint32_t, 72> &types,
                        const AudioDeviceId &device, AudioCreationDevice &api, const AssetStore &assets,
                        const std::uint32_t &spatial_enabled)
{
    driver.samples = {};
    storage.samples = {};
    for (const auto &definition : audio_sample_definitions)
    {
        const auto slot = definition.slot;
        if (spatial_enabled == 0)
            types[slot] = 1;
        else if (types[slot] == 2)
            load_audio_sample(driver.samples[slot], storage.samples[slot], device, api, assets, definition.resource,
                              spatial_enabled, 0x20012);
    }
    for (const auto &definition : audio_sample_definitions)
        if (types[definition.slot] == 1)
            load_audio_sample(driver.samples[definition.slot], storage.samples[definition.slot], device, api, assets,
                              definition.resource, spatial_enabled, 2);
    return true;
}
bool initialize_audio_primary(AudioDeviceId device, AudioPrimaryId &primary, AudioCreationDevice &api,
                              std::uint32_t requested_rate, std::uint32_t &capability, std::uint32_t &enabled)
{
    AudioBufferId primary_buffer{primary.value};
    const auto created = api.create_buffer(device, {20, 0x11, 0, 0, {}}, primary_buffer);
    primary.value = primary_buffer.value;
    if (created != 0)
        return false;
    std::array<std::uint32_t, 24> caps{};
    caps[0] = 96;
    if (api.capabilities(device, caps) != 0)
        return false;
    if (caps[11] < 8)
    {
        capability = 0;
        enabled = 0;
    }
    AudioWaveFormat format{};
    std::uint32_t written{};
    if (api.primary_format(primary, format, 18, written) != 0)
        return false;
    format.tag = 1;
    format.samples_per_second = requested_rate;
    format.bits_per_sample = (caps[1] & 8) ? 16u : 8u;
    format.block_align = static_cast<std::uint16_t>((std::uint32_t(format.channels) * format.bits_per_sample) >> 3);
    format.bytes_per_second = std::uint32_t(format.block_align) * requested_rate;
    return api.set_primary_format(primary, format) == 0;
}
} // namespace sgn::retail
