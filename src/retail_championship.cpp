#include "sgn/retail_championship.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/little_endian_words.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using sgn::little_endian::read_le32;
void store(std::span<std::byte> bytes, std::size_t at, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFFu);
}
using sgn::bit_reinterpret::as_signed;
std::uint32_t update_record(std::span<std::byte, 8> record, CampaignInitials initials, std::uint32_t value,
                            bool minimum)
{
    const auto old = as_signed(read_le32(record, 0));
    const bool changed = minimum ? as_signed(value) < old : as_signed(value) > old;
    if (!changed)
        return 0;
    record[7] = std::byte{1};
    store(record, 0, value);
    // 437D34/437D74 read three name bytes after publishing value/flag.
    const std::array name{initials[0], initials[1], initials[2]};
    std::copy(name.begin(), name.end(), record.begin() + 4);
    return 1;
}
void mark_unlocked(std::span<std::byte, 1> flag)
{
    if (flag[0] == std::byte{0})
        flag[0] = std::byte{2};
}
} // namespace

std::uint32_t update_campaign_records_44cd10(CampaignTrackRecords records, CampaignInitials initials,
                                             const std::array<std::uint32_t, 4> &values)
{
    update_record(records.subspan<0, 8>(), initials, values[0], true);
    update_record(records.subspan<8, 8>(), initials, values[1], true);
    update_record(records.subspan<16, 8>(), initials, values[2], false);
    return update_record(records.subspan<24, 8>(), initials, values[3], false);
}

void sort_campaign_standings_44be80(CampaignStandings standings, std::uint32_t count)
{
    if (count > 6)
        throw std::invalid_argument("campaign standings count exceeds six rows");
    for (auto remaining = count; remaining > 1; --remaining)
    {
        for (std::uint32_t row = 0; row + 1 < remaining; ++row)
        {
            const std::size_t at = static_cast<std::size_t>(row) * 0x20;
            auto difference = read_le32(standings, at + 8) - read_le32(standings, at + 0x28);
            if (difference == 0)
                difference = read_le32(standings, at + 4) - read_le32(standings, at + 0x24);
            if (difference == 0)
                difference = read_le32(standings, at + 0x10) - read_le32(standings, at + 0x30);
            if (as_signed(difference) < 0)
                std::swap_ranges(standings.begin() + static_cast<std::ptrdiff_t>(at),
                                 standings.begin() + static_cast<std::ptrdiff_t>(at + 0x20),
                                 standings.begin() + static_cast<std::ptrdiff_t>(at + 0x20));
        }
    }
}

std::uint32_t campaign_player_rank_44ccf0(std::span<const std::byte, 0xC0> standings)
{
    for (std::uint32_t row = 0; row < 6; ++row)
        if (read_le32(standings, static_cast<std::size_t>(row) * 0x20) != 0)
            return row + 1;
    return 0;
}
std::uint32_t campaign_checkpoint_44cc90(std::span<const std::byte, 0x10> checkpoints, std::uint32_t race)
{
    if (as_signed(race) <= 1)
        return 0;
    for (std::size_t at = 0; at < checkpoints.size(); at += 4)
        if (read_le32(checkpoints, at) == race)
            return 1;
    return 0;
}
void commit_campaign_44ccc0(std::span<const std::byte, 0x160> live, CampaignLive saved_slot)
{
    std::copy(live.begin(), live.end(), saved_slot.begin());
}
void unlock_campaign_car_437880(std::span<std::byte, 1> flag)
{
    mark_unlocked(flag);
}
void unlock_campaign_track_4378a0(std::span<std::byte, 1> flag)
{
    mark_unlocked(flag);
}
void unlock_campaign_class_car_4378c0(std::span<std::byte, 52> unlocks, std::uint32_t category)
{
    // Jump-table 437900..437917 and full 4378C0..4378FD, including invalid -> 0.
    constexpr std::array<std::uint32_t, 6> cars{2, 6, 3, 14, 16, 5};
    const auto car = category >= 1 && category <= 6 ? cars[category - 1] : 0;
    mark_unlocked(std::span<std::byte, 1>(unlocks.subspan(car, 1)));
}

CampaignSettlementResult settle_campaign_race_prefix_44c0a0(CampaignSettlementViews view)
{
    if (view.progression != 10)
        throw std::invalid_argument("campaign settlement requires source state 10");
    if (read_le32(view.results, 0) != 0)
    {
        view.progression = 24;
        return {CampaignSettlementBoundary::aborted};
    }
    // 439E90's mode-5 producer bounds successful finish places to 1..6.
    for (std::size_t row = 0; row < 6; ++row)
    {
        const auto place = read_le32(view.results, 4 + row * 0x14);
        if (place < 1 || place > 6)
            throw std::invalid_argument("campaign result place outside source producer range");
    }
    const bool bonus = as_signed(read_le32(view.live, 8)) > as_signed(view.last_normal_race);
    const std::uint32_t multiplier = bonus ? 3u : 1u;
    const std::array values{read_le32(view.results, 8), read_le32(view.results, 12), read_le32(view.results, 16),
                            read_le32(view.results, 20)};
    update_campaign_records_44cd10(view.track_records, view.live.subspan<4, 3>(), values);
    for (std::size_t row = 0; row < 6; ++row)
    {
        const auto place = read_le32(view.results, 4 + row * 0x14);
        const auto at = 0x100 + row * 0x10;
        store(view.live, at + 12, 7u - place); // complete 43DC20, EDX=6
        if (!bonus || row == 0)
        {
            store(view.live, at, read_le32(view.live, at) + (place == 1 ? 1u : 0u));
            store(view.live, at + 4, read_le32(view.live, at + 4) + read_le32(view.awards, place * 0x10) * multiplier);
        }
    }
    const auto player_place = read_le32(view.results, 4);
    const auto reward = read_le32(view.awards, player_place * 0x10 + (bonus ? 12u : 4u)) + values[3];
    if (bonus)
        unlock_campaign_car_437880(std::span<std::byte, 1>(view.unlocks.subspan(player_place == 1 ? 18u : 17u, 1)));
    store(view.live, 12, read_le32(view.live, 12) + reward);
    return {CampaignSettlementBoundary::presentation_required, multiplier, reward};
}
} // namespace sgn::retail
