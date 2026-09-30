#pragma once
#include "sgn/source_allocation_bank.hpp"
#include <array>
namespace sgn::retail::source_submission_detail
{
// Pinned original477CD0, seventeen64-byte rows; no fan/strip reconstruction.
inline constexpr std::array<std::array<std::uint32_t, 16>, 17> strip{{
    {{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 3u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 4u, 3u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 5u, 3u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 6u, 3u, 5u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 7u, 3u, 6u, 4u, 5u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 8u, 3u, 7u, 4u, 6u, 5u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 9u, 3u, 8u, 4u, 7u, 5u, 6u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 10u, 3u, 9u, 4u, 8u, 5u, 7u, 6u, 0u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 11u, 3u, 10u, 4u, 9u, 5u, 8u, 6u, 7u, 0u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 12u, 3u, 11u, 4u, 10u, 5u, 9u, 6u, 8u, 7u, 0u, 0u, 0u}},
    {{1u, 0u, 2u, 13u, 3u, 12u, 4u, 11u, 5u, 10u, 6u, 9u, 7u, 8u, 0u, 0u}},
    {{1u, 0u, 2u, 14u, 3u, 13u, 4u, 12u, 5u, 11u, 6u, 10u, 7u, 9u, 8u, 0u}},
    {{1u, 0u, 2u, 15u, 3u, 14u, 4u, 13u, 5u, 12u, 6u, 11u, 7u, 10u, 8u, 9u}},
}};
inline std::uint32_t strip_index(std::uint32_t count, std::uint32_t ordinal)
{
    if (count >= strip.size() || ordinal >= count)
        throw SourceAllocationError(0x433ae6, "alpha strip reads unrepresented adjacent PE data");
    return strip[count][ordinal];
}
// Actual433C20 byte selector and433C08 jump table for mode&78.
inline constexpr std::array<std::array<std::uint32_t, 2>, 16> blends{{
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{5u, 6u}},
    {{5u, 6u}},
    {{2u, 2u}},
    {{5u, 2u}},
    {{5u, 2u}},
    {{9u, 1u}},
    {{5u, 2u}},
}};
inline std::array<std::uint32_t, 2> blend_for_mode(std::uint32_t mode)
{
    return blends[(mode & 0x78u) >> 3];
}
} // namespace sgn::retail::source_submission_detail
