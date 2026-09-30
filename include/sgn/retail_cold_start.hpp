#pragma once
#include "sgn/contact_solver.hpp"

namespace sgn::retail
{
// Borrowed owners only. Model bindings, visual storage and the two additional
// original allocation strides are outside this numeric initialization component.
struct ColdNumericStorage
{
    std::span<VehicleParameters, 6> parameters;
    std::span<VehicleState, 6> physics;
    std::span<VehicleRaceState, 6> race;
    std::span<contact::solver::State, 6> contact;
    unsigned &current_slot;
};
// 0040A2F0: clear the represented fields in all six physical vehicles, then
// initialize slot zero. This is not the complete model/world startup procedure.
// wheel_load is the source value at 00482670, supplied by its world owner.
void initialize_cold_numeric_storage(ColdNumericStorage, float wheel_load);
Curve cold_torque_curve(); // Full 0040A160; source constants, binary64 temporaries.
} // namespace sgn::retail
