#pragma once
#include "sgn/contact_solver.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/retail_spline.hpp"
#include <optional>

namespace sgn::retail
{
// One world-owned instance, shared by the lifecycle and mode transitions.
struct VehicleResetGlobals
{
    std::uint32_t transition_active{}, special_value{}, special_timer{}; // 4D5CA4/A8/AC
};
struct ResetVehicle
{
    VehicleState &physics;
    VehicleRaceState &race;
    contact::solver::State &contact;
    contact::player::VehicleId id{};
};
struct VehicleResetParameters
{
    std::int32_t stage{}, mode{}, selector{}; // 60F2B4,60F2B0,60DF98
    float normal_energy_cost{}, upgraded_energy_cost{}, course_length{};
    // Literal DWORD at [624474]+24, not descriptor.point_count (+8).
    // Some PMDs end before these adjacent bytes. 418D20 reads them only
    // after saved_24a4!=0; absence is an unresolved source-memory frontier.
    std::optional<std::uint32_t> spacing_word;
    SplineView spline;
    RespawnMask mask;
    VehicleVector unit_gravity;
    std::uint32_t tick{};
    std::array<std::string_view, 6> vehicle_names;
    std::string_view special_end_text, retired_text;
};
struct VehicleResetWorld
{
    std::span<ResetVehicle, 6> vehicles;
    unsigned current{};
    contact::player::Controls &controls;
    VehicleResetGlobals &globals;
    const VehicleResetParameters &parameters;
};
void consume_respawn_energy(ResetVehicle &, const VehicleResetParameters &); // 00418710
bool exhausted_and_still(const VehicleState &);                              // 00418760
void retire_current_vehicle(VehicleResetWorld &);                            // 004186C0, 00418660 and 00443700
void check_vehicle_retirement(VehicleResetWorld &);                          // 004187A0
void clear_special_globals(VehicleResetGlobals &) noexcept;                  // 00418D10
void prepare_special_respawn(VehicleResetWorld &, unsigned target);          // 00418D20 and 00447AA0
void end_vehicle_special_mode(VehicleResetWorld &, unsigned target);         // 00418D90 and full callee chain
// Return value is new diagnostics: true only if the source placement path ran.
bool reset_vehicle(VehicleResetWorld &, unsigned target); // 00412940 and full callee chain
} // namespace sgn::retail
