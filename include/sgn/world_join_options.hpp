#pragma once
#include <cstdint>
#include <string_view>
#include <limits>

namespace sgn::diagnostics
{
struct WorldJoinSelection
{
    std::uint64_t first{};
    std::uint32_t count{};
    bool valid() const noexcept
    {
        return first > 0 && count > 0 && count <= 64 &&
               first <= (std::numeric_limits<std::uint64_t>::max)() - (count - 1);
    }
    bool contains(std::uint64_t serial) const noexcept
    {
        return valid() && serial >= first && serial - first < count;
    }
};
// Explicit host diagnostic window. No tick/front inference or activation default.
WorldJoinSelection parse_world_join_selection(std::wstring_view first, std::wstring_view count);
} // namespace sgn::diagnostics
