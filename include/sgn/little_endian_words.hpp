#pragma once
// Shared little-endian 32-bit word access over byte spans (code organisation only, no new behaviour).
//     read_le32(bytes, at)          bytes[at..at+3] as a little-endian word          (was get)
//     write_le32(bytes, at, value)  store value little-endian into bytes[at..at+3]  (was put)
// No bounds check beyond std::span::operator[], exactly like the per-file copies they replace.
// Copies that throw on a short span (e.g. "PageMain object read exceeds source owner") are
// different helpers and stay local to their file.
// A file adopts a helper with `using sgn::little_endian::read_le32;` (etc.) at the place of its
// former local helper.
#include <cstddef>
#include <cstdint>
#include <span>

namespace sgn::little_endian
{
inline std::uint32_t read_le32(std::span<const std::byte> bytes, std::size_t at)
{
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= std::to_integer<std::uint32_t>(bytes[at + i]) << (8 * i);
    return value;
}
inline void write_le32(std::span<std::byte> bytes, std::size_t at, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = static_cast<std::byte>((value >> (8 * i)) & 255u);
}
} // namespace sgn::little_endian
