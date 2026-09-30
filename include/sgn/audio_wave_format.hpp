#pragma once
#include <cstdint>

namespace sgn::retail
{
// The fields of the source 18-byte WAVEFORMATEX. Serialize explicitly; the
// native C++ structure's padding is not the original memory layout.
struct AudioWaveFormat
{
    std::uint16_t tag{}, channels{};
    std::uint32_t samples_per_second{}, bytes_per_second{};
    std::uint16_t block_align{}, bits_per_sample{}, extra_bytes{};
};
struct AudioWaveChunk
{
    std::uint32_t id{}, size{}, type{}, data_offset{}, flags{};
};
} // namespace sgn::retail
