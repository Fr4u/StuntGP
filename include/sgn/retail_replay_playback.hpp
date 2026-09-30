#pragma once
#include "sgn/retail_replay.hpp"
#include "sgn/retail_replay_controls.hpp"
#include "sgn/vehicle_air_tick.hpp"
#include "sgn/vehicle_dynamics_shared.hpp"

namespace sgn::retail
{
struct ReplayPlaybackVehicle
{
    const VehicleParameters &parameters;
    VehicleState &physics;
    contact::solver::State &contact;
    std::span<VehicleWheelAnimation, 4> wheels;
    VehicleAirTickState &air;
};
struct ReplayPlaybackContext
{
    float gravity_magnitude;
    const VehicleDynamicsWorkspace &dynamics;
    const VehicleAirTickContext &air;
};
struct ReplayPlaybackObservation
{
    bool decoded{}, advanced{}, stopped{};
    VehicleAirTickObservation air;
};
VehicleAirTickObservation replay_air_dynamics(ReplayPlaybackVehicle, const ReplayPlaybackContext &); // full 00419EC0
// Full 0041AB70. Source ECX selects the recording, while the global current
// vehicle owns decoded physics; these identities need not be silently conflated.
ReplayPlaybackObservation play_replay_frame(ReplayBank &, const VehicleRaceState &recording_owner, ReplayWorkspace &,
                                            ReplayPlaybackVehicle, const ReplayPlaybackContext &);
// Full 0041AF50 wrapper; an absent link or status !=1 leaves all state untouched.
ReplayPlaybackObservation update_replay_vehicle(ReplayBank &, const VehicleRaceState &current, ReplayWorkspace &,
                                                ReplayPlaybackVehicle, const ReplayPlaybackContext &);
} // namespace sgn::retail
