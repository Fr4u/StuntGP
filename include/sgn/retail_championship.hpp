#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sgn::retail
{
// Borrowed views only. PersistenceBank owns saved slots, track records and
// unlock bytes; the frontend owns live campaign and presentation standings.
// Every address below refers to the pinned D3D executable. See
// docs/CHAMPIONSHIP-RECOVERY.md for instruction ranges and recovery limits.
using CampaignLive = std::span<std::byte, 0x160>;        // 5F92DC
using CampaignStandings = std::span<std::byte, 0xC0>;    // 5F9AA4, six rows of 0x20
using CampaignTrackRecords = std::span<std::byte, 0x20>; // 5E0F7C + track * 0x20
using CampaignInitials = std::span<const std::byte, 3>;

// 44CD10 -> 437D60 twice, 437D20 twice. Signed comparisons; equal values
// preserve the old initials and changed flag. Return is the final call's EAX.
std::uint32_t update_campaign_records_44cd10(CampaignTrackRecords, CampaignInitials,
                                             const std::array<std::uint32_t, 4> &);

// Source bubble sort: points, wins, then grid/order, all descending through
// wrapped 32-bit subtraction followed by TEST/JGE. Preserve stable equal rows.
// count is bounded to the six-row retail frontend buffer by the host adapter.
void sort_campaign_standings_44be80(CampaignStandings, std::uint32_t count = 6);
std::uint32_t campaign_player_rank_44ccf0(std::span<const std::byte, 0xC0>);
std::uint32_t campaign_checkpoint_44cc90(std::span<const std::byte, 0x10>, std::uint32_t race);
void commit_campaign_44ccc0(std::span<const std::byte, 0x160> live, CampaignLive saved_slot);

// Unlock state 0 -> 2 only. Values already nonzero are preserved, including 1
// (notification consumed) and 2 (notification pending). Selection of the byte
// is an explicit borrow from PersistenceBank::unlocks(), never another owner.
void unlock_campaign_car_437880(std::span<std::byte, 1>);
void unlock_campaign_track_4378a0(std::span<std::byte, 1>);
void unlock_campaign_class_car_4378c0(std::span<std::byte, 52>, std::uint32_t category);

enum class CampaignSettlementBoundary
{
    aborted,
    presentation_required
};
struct CampaignSettlementViews
{
    CampaignLive live;
    CampaignTrackRecords track_records;
    std::span<std::byte, 52> unlocks;
    std::span<const std::byte, 0x7C> results; // 61BED0: abort + six rows of 0x14
    std::span<const std::byte, 0x70> awards;  // 5F9498, seven rows of 0x10
    std::uint32_t last_normal_race;           // 5F943C
    std::uint32_t &progression;               // borrowed 5E3794
};
struct CampaignSettlementResult
{
    CampaignSettlementBoundary boundary;
    std::uint32_t points_multiplier{};
    std::uint32_t player_reward{};
};
// Exact state-10 prefix of 44C0A0: dispatch + 44C507..44C61B and complete
// 44CD10/437D60/437D20/43DC20/437880 callees. Stops BEFORE 44BD00 at 44C61C.
// On success progression stays 10: the source standings/presentation calls
// must complete before 44C6F8 publishes 11. This is not the full state machine.
// The aborted branch publishes 24 and returns. Invalid host inputs fail before
// any mutation; these checks are new host boundaries, not recovered behavior.
CampaignSettlementResult settle_campaign_race_prefix_44c0a0(CampaignSettlementViews);
} // namespace sgn::retail
