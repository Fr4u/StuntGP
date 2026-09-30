#pragma once
#include "sgn/race_session_vehicle_tick.hpp"
#include "sgn/retail_replay_playback.hpp"

namespace sgn::retail
{
struct VehicleWorldTickAudit
{
    VehicleTickAudit *vehicle{};
    void *context{};
    void (*before_stage)(void *, std::uint32_t source_entry, unsigned slot) noexcept {};
};
// Complete418970 source order, using the actual session's shared resources.
// Candidate until its original full-callee comparison is accepted.
void tick_session_replay_visuals(RaceSessionState &, const RaceSessionVehicleResources &,
                                 VehicleTickAudit *audit = nullptr);
// Actual implementations for all WorldSteps callbacks of418A90. Neither this
// adapter nor world_tick advances the global tick or chooses a display rate.
// Resources reset/projection values are refreshed from their owners per call.
void tick_session_vehicle_world(RaceSessionState &, const RaceSessionVehicleResources &,
                                VehicleWorldTickAudit *audit = nullptr);
} // namespace sgn::retail
