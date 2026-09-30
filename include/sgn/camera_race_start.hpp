#pragma once
#include "sgn/contact_player.hpp"
#include <span>

namespace sgn::retail
{
// Invocation-only observations/settings view. The caller publishes modified
// vehicle camera settings to their sole owner after the call; physical values
// remain observations. The reset binding must reach the actual vehicle owner.
struct CameraRaceStartView
{
    contact::player::Controls &controls;
    std::span<CameraVehicle, 6> vehicles;
    std::uint32_t &selected_preset;
    std::uint32_t split_mode;
    CameraModeReset effect_reset;
    CameraMatrix &shared_model_identity; // borrowed sole61C468, required
};
// Full402710 +4026E0. Presets use the controller current on entry, even when
// another controller is bound. Current becomes zero only at the final402580.
void activate_race_cameras(CameraRaceStartView, std::span<const std::uint32_t, 4> selected_vehicles);
// Full403F40 with the destination equal to this camera's position. Original
// local-stack position.w is undefined; the native policy preserves its bits.
void place_race_camera_position(CameraState &, const CameraVehicle &);
// Full404280. Preset target can differ from the current controller's vehicle;
// placement reads the latter. False means the original early return was taken.
bool initialize_race_camera(CameraRaceStartView, std::uint32_t preset_vehicle);
} // namespace sgn::retail
