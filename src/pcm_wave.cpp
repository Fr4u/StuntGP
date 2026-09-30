#include "sgn/platform_audio.hpp"
#include <cstring>
#include <limits>

namespace sgn
{
namespace
{
std::uint32_t u32(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + at, 4);
    return value;
}
std::uint16_t u16(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint16_t value;
    std::memcpy(&value, bytes.data() + at, 2);
    return value;
}
bool tag(std::span<const std::uint8_t> bytes, std::size_t at, const char *text)
{
    return std::memcmp(bytes.data() + at, text, 4) == 0;
}
} // namespace
PcmWave decode_pcm_wave(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 12 || !tag(bytes, 0, "RIFF") || !tag(bytes, 8, "WAVE"))
        throw FormatError("expected RIFF WAVE audio");
    const std::uint64_t limit = std::uint64_t(u32(bytes, 4)) + 8;
    if (limit != bytes.size())
        throw FormatError("WAVE RIFF size differs from resource");
    PcmWave wave;
    bool format = false, data = false;
    for (std::size_t at = 12; at < bytes.size();)
    {
        if (bytes.size() - at < 8)
            throw FormatError("truncated WAVE chunk header");
        const auto count = u32(bytes, at + 4);
        const auto payload = at + 8;
        if (count > bytes.size() - payload)
            throw FormatError("WAVE chunk outside resource");
        if (tag(bytes, at, "fmt "))
        {
            if (format || count < 16 || u16(bytes, payload) != 1)
                throw FormatError("expected one PCM WAVE format");
            wave.channels = u16(bytes, payload + 2);
            wave.sample_rate = u32(bytes, payload + 4);
            wave.block_align = u16(bytes, payload + 12);
            wave.bits_per_sample = u16(bytes, payload + 14);
            if ((wave.channels != 1 && wave.channels != 2) ||
                (wave.bits_per_sample != 8 && wave.bits_per_sample != 16) || wave.sample_rate < 1000 ||
                wave.sample_rate > 200000)
                throw FormatError("unsupported PCM WAVE channel/rate/bit depth");
            if (wave.block_align != wave.channels * (wave.bits_per_sample / 8) ||
                u32(bytes, payload + 8) != wave.sample_rate * wave.block_align)
                throw FormatError("inconsistent PCM WAVE stride or byte rate");
            format = true;
        }
        else if (tag(bytes, at, "data"))
        {
            if (data || count == 0)
                throw FormatError("expected one nonempty PCM WAVE data chunk");
            wave.samples.assign(bytes.begin() + payload, bytes.begin() + payload + count);
            data = true;
        }
        const std::uint64_t next = std::uint64_t(payload) + count + (count & 1u);
        if (next > bytes.size())
            throw FormatError("missing WAVE chunk padding");
        at = static_cast<std::size_t>(next);
    }
    if (!format || !data || wave.samples.size() % wave.block_align)
        throw FormatError("incomplete PCM WAVE sample frames");
    return wave;
}
} // namespace sgn
