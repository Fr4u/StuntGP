#include "sgn/world_join_options.hpp"
#include <limits>
#include <stdexcept>

namespace sgn::diagnostics
{
namespace
{
std::uint64_t decimal(std::wstring_view text)
{
    if (text.empty())
        throw std::invalid_argument("world join range requires unsigned decimal values");
    std::uint64_t out = 0;
    for (const auto c : text)
    {
        if (c < L'0' || c > L'9')
            throw std::invalid_argument("world join range requires unsigned decimal values");
        const auto digit = static_cast<std::uint64_t>(c - L'0');
        if (out > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10)
            throw std::invalid_argument("world join range overflows uint64");
        out = out * 10 + digit;
    }
    return out;
}
} // namespace
WorldJoinSelection parse_world_join_selection(std::wstring_view first, std::wstring_view count)
{
    const auto begin = decimal(first), size = decimal(count);
    if (begin == 0 || size == 0 || size > 64 || begin > (std::numeric_limits<std::uint64_t>::max)() - (size - 1))
        throw std::invalid_argument("world join range requires first>=1, count1..64 and no overflow");
    return {begin, static_cast<std::uint32_t>(size)};
}
} // namespace sgn::diagnostics
