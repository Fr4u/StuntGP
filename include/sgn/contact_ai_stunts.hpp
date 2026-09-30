#pragma once
#include "sgn/contact_solver.hpp"
#include "sgn/vehicle_ai_automatic.hpp"

namespace sgn::retail
{
// Single owner of the three additional per-car words. 2674 remains in
// AiAutomaticMemory and 2678 remains VehicleState.stunt_meter.
struct AiStuntHelperState
{
    std::uint32_t input_kind{}, grade{}; // 267C/2680
    float planned_route_distance{};      // 2684
};
struct AiStuntKnot
{
    float progress{}, width{};
    std::uint32_t grade{};
};
// Source 499FB8 has 33 rows; valid authored tables retain a zero terminator.
using AiStuntKnots = std::array<AiStuntKnot, 33>;
struct AiStuntContext
{
    std::uint32_t tick{};
    Vec3 world_up{};
};
void advance_ai_stunt(const VehicleParameters &, VehicleState &, AiStuntHelperState &,
                      const AiStuntContext &); // 00406EA0 + 00406E80 + real0040D090
void choose_ai_stunt(AiAutomaticVehicle, contact::solver::State &, AiStuntHelperState &, const AiAutomaticGlobals &,
                     const AiStuntKnots &, VehicleRandom &, const AiStuntContext &); // full00407010
AiStuntKnots decode_ai_stunt_knots(std::span<const std::uint8_t>);                   // authored 004467B0 contract
} // namespace sgn::retail
