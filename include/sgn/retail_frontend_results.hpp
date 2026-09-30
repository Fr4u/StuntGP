#pragma once
#include "sgn/retail_level_selection.hpp"
#include "sgn/retail_persistence.hpp"

namespace sgn::retail
{
// Invocation-only views of the actual source roster and result destinations.
// Indices1/3/4/5/6/7 correspond to the selector5DF528, not the race game_type.
struct FrontendReturnBuffers
{
    std::span<const std::uint8_t> roster;
    std::span<std::uint8_t> result;
};
struct FrontendRaceReturnWorld
{
    std::span<std::uint8_t, persistence_size> persistence;
    const LevelOverrideOptions &options;
    std::span<const SelectionRecord, 6> selection;
    const TimeTrialSelectionState &trial;
    std::uint32_t &selector; // 5DF528
    std::array<FrontendReturnBuffers, 8> buffers;
};
// Pointer-valued source result+8 uses a nonzero native recording identity.
// This is not a host address. Zero remains the source null identity.
std::uint32_t frontend_replay_identity(std::optional<ReplayHandle>);
std::optional<ReplayHandle> frontend_replay_handle(std::uint32_t identity);
// Full439E90: settings writeback plus all seven switch slots, including the
// original default slot2. This does not award championship points or unlocks.
void return_frontend_race_result(FrontendRaceReturnWorld &);
} // namespace sgn::retail
