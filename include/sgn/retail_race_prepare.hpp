#pragma once
#include "sgn/retail_vehicle_storage.hpp"

namespace sgn::retail
{
struct RaceStartupCounters
{
    std::uint32_t field_60d71c{};
    // Only the three written words at 485650 + i*1C. The four following
    // words in each source record are outside the reset's ownership.
    std::array<std::array<std::uint32_t, 3>, 4> fields_485650{};
};
void clear_race_startup_counters(RaceStartupCounters &) noexcept;                       // full449C60
void reset_current_race_hud(VehicleStorage &, unsigned current, RaceStartupCounters &); // full443D90
void activate_vehicle_for_race(VehicleRaceState &, VehicleRaceStartupState &) noexcept; // full40D410
void prepare_vehicles_for_race(VehicleStorage &) noexcept;                              // full40D430
float measure_race_spline_length(SplineView); // full428840; excludes last-to-first segment
void reset_all_race_records(VehicleStorage &, unsigned &current, RaceClockState &, const SplineSet &); // full41D980
} // namespace sgn::retail
