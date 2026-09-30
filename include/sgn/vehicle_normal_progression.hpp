#pragma once
#include "sgn/vehicle_prestart_progression.hpp"
namespace sgn::retail
{
struct NormalRaceProgression
{
    VehicleRaceFinishWorld &finish;
    std::span<VehicleProgressionState, 6> progressions;
    std::uint32_t split_setting{};                                // 576BE0
    std::string_view last_lap_text, leading_text, following_text; // 612858/810/80C
};
void announce_normal_checkpoint(const VehicleRaceState &, const VehicleProgressionState &,
                                const VehicleProgressionContext &, const NormalRaceProgression &); // 41D5C0
void check_normal_checkpoints(const VehicleRaceState &, VehicleProgressionState &, const VehicleProgressionContext &,
                              const NormalRaceProgression &); // 41D760 mode2
void complete_normal_lap(const VehicleState &, VehicleRaceState &, VehicleProgressionState &, AiAutomaticMemory &,
                         const VehicleProgressionContext &, NormalRaceProgression &); // 41DD60 mode2
VehicleProgressionObservation tick_normal_progression(const VehicleState &, VehicleRaceState &,
                                                      VehicleProgressionState &, AiAutomaticMemory &,
                                                      const VehicleProgressionContext &,
                                                      NormalRaceProgression &); // 41E210 mode2
} // namespace sgn::retail
