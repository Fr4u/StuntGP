#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace sgn::retail
{
inline constexpr std::size_t campaign_data_size = 0x660;
inline constexpr std::size_t campaign_numeric_token_size = 0x100;
// Borrowed frontend owners only. Award row zero overlaps sponsor rows in rules.
// numeric_token is the shared 43C770 working buffer, outside the cold clear.
struct CampaignDataViews
{
    std::span<std::byte, campaign_data_size> rules;                  // 5F943C..5F9A9B
    std::span<std::byte, campaign_numeric_token_size> numeric_token; // 5FD148..5FD247
};
// Full 435B60..435E2F; source-compatible bounded calls always return 1.
// The source consumes rows in literal order and ignores label comparison.
// Failed %d conversion retains the old word. Both owners and the cursor are
// retained across invocations. End of input is an implicit NUL sentinel.
// New host policy: invalid cursor, overlapping owners/input, buffer overflow
// or an unbounded numeric C string throw std::invalid_argument atomically.
std::uint32_t parse_campaign_data_435b60(CampaignDataViews, std::string_view input, std::uint32_t &cursor);
} // namespace sgn::retail
