#pragma once
#include "sgn/retail_camera.hpp"
namespace sgn::retail
{
// Persistent per-car camera fields: fifteen original DWORDs. CameraVehicle is
// the invocation snapshot; physical fields and22C8 belong to physics/race.
struct VehicleCameraSettings
{
    CameraVector orbit_offset{}, target_offset{}, angles{}; // 22CC,22DC,22EC
    float distance{}, approach_scale{};                     // 22FC,2300
    std::uint32_t adaptive_distance{};                      // 2304
};
// 40A2F0's four camera writes. The surrounding owner performs the complete clear.
inline void initialize_vehicle_camera_settings(VehicleCameraSettings &s) noexcept
{
    s.angles.x = 0;
    s.angles.y = 180;
    s.angles.z = 0;
    s.distance = 1000;
}
inline void read_vehicle_camera_settings(CameraVehicle &v, const VehicleCameraSettings &s,
                                         std::uint32_t saved_mode) noexcept
{
    v.orbit_offset = s.orbit_offset;
    v.target_offset = s.target_offset;
    v.angles = s.angles;
    v.distance = s.distance;
    v.approach_scale = s.approach_scale;
    v.adaptive_distance = s.adaptive_distance;
    v.camera_mode = saved_mode;
}
inline void write_vehicle_camera_settings(VehicleCameraSettings &s, std::uint32_t &saved_mode,
                                          const CameraVehicle &v) noexcept
{
    s.orbit_offset = v.orbit_offset;
    s.target_offset = v.target_offset;
    s.angles = v.angles;
    s.distance = v.distance;
    s.approach_scale = v.approach_scale;
    s.adaptive_distance = v.adaptive_distance;
    saved_mode = v.camera_mode;
}
} // namespace sgn::retail
