#pragma once
#include "sgn/retail_camera.hpp"

namespace sgn::retail
{
struct VehicleState;
struct VehicleParameters;

// Fields owned by the race/vehicle lifecycle, outside the current physics state.
// All coordinates and progress values retain the original units and axes.
struct CameraVehicleObservation
{
    std::uint32_t active{}; // vehicle +0x1ba8; only 1 is active
    float progress{};       // vehicle +0x23b0
    float contact_height{}; // vehicle +0x7238
};
// Refresh only physical observations. Preset/orbit state remains camera-owned.
void camera_observe_vehicle(CameraVehicle &, const VehicleParameters &, const VehicleState &,
                            const CameraVehicleObservation &) noexcept;

// Slots may refer to VehicleState embedded in separate race objects. Keep this
// object and its pointers alive and unchanged while the returned callback runs.
struct CameraEffectTargets
{
    std::array<VehicleState *, 6> vehicles{};
    std::array<std::uint32_t, 6> active{};
};
// Reject missing active targets before exposing a callback or changing state.
CameraModeReset camera_vehicle_reset_adapter(CameraEffectTargets &);
} // namespace sgn::retail
