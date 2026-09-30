#pragma once
#include <cstdint>

namespace sgn::retail::source_462890
{
// Source register values, never host pointers. Arithmetic is modulo2^32;
// admitted owner bounds are checked only by a reached memory read.
constexpr std::uint32_t add(std::uint32_t value, std::uint32_t amount) noexcept
{
    return value + amount;
}
constexpr std::uint32_t subtract(std::uint32_t value, std::uint32_t amount) noexcept
{
    return value - amount;
}
constexpr std::uint32_t difference(std::uint32_t address, std::uint32_t base) noexcept
{
    return address - base;
}
constexpr bool needle_byte_site(std::uint32_t site) noexcept
{
    return site == 0x462897 || site == 0x4628a1 || site == 0x4628d4 || site == 0x4628e4;
}
constexpr bool haystack_byte_site(std::uint32_t site) noexcept
{
    return site == 0x4628ae || site == 0x4628b9 || site == 0x4628ca || site == 0x4628db || site == 0x4628eb ||
           site == 0x464f78;
}
constexpr bool haystack_dword_site(std::uint32_t site) noexcept
{
    return site == 0x464f96 || site == 0x464fd8;
}
static_assert(add(0xffffffffu, 2) == 1 && subtract(0, 1) == 0xffffffffu);
} // namespace sgn::retail::source_462890
