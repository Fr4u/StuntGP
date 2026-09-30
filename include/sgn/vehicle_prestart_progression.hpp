#pragma once
#include "sgn/vehicle_progression.hpp"

namespace sgn::retail
{
// World owners for finite-race completion. Distinct from lap records and the
// live ranking. The source allocates sixteen result identities, not six.
struct VehicleRaceFinishState
{
    std::array<std::optional<unsigned>, 16> order{};      // 612D54..612D90
    std::uint32_t count{};                                // 612D94
    std::uint32_t best_total_ticks{}, best_total_score{}; // 60F004/60F00C
};
struct UnfinishedVehicleCounts
{
    unsigned upgraded{}, other{};
};
UnfinishedVehicleCounts count_unfinished_vehicles(std::span<const VehicleState, 6>,
                                                  std::span<const VehicleRaceState, 6>,
                                                  std::uint32_t hud_block);    // 41D9F0
unsigned count_active_vehicles(std::span<const VehicleRaceState, 6>) noexcept; // 41DA50
struct VehicleRaceFinishWorld
{
    std::span<VehicleState, 6> physical;
    std::span<VehicleRaceState, 6> race;
    const RaceRanking &ranking;
    VehicleRaceFinishState &finish;
    const RaceClockState &clock;
    const ProgressionRecordState &records;
    const std::uint32_t &hud_block; // 61299C, exactly1 overrides second count
};
void finish_vehicle_race(VehicleRaceFinishWorld &, unsigned current); // full41DA70
void complete_prestart_lap(const VehicleState &, VehicleRaceState &, VehicleProgressionState &, AiAutomaticMemory &,
                           const VehicleProgressionContext &,
                           VehicleRaceFinishWorld &); // 41DD60 mode1, including finite finish
void check_prestart_checkpoints(const VehicleRaceState &, VehicleProgressionState &,
                                const VehicleProgressionContext &); // 41D760 mode1
VehicleProgressionObservation tick_prestart_progression(const VehicleState &, VehicleRaceState &,
                                                        VehicleProgressionState &, AiAutomaticMemory &,
                                                        const VehicleProgressionContext &,
                                                        VehicleRaceFinishWorld &); // 41E210 mode1
} // namespace sgn::retail
