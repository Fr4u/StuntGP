#pragma once
#include "sgn/race_session.hpp"
#include "sgn/retail_camera_vehicle.hpp"

namespace sgn::retail
{
// Full443DD0: select mode10 on the actual current controller through the
// existing full4028D0/412910 helper. No viewport, lens or camera-path advance.
inline void finish_race_camera(RaceSessionState &s)
{
    if (s.controls.current >= s.controls.controllers.size())
        throw FormatError("race finish camera controller outside source slots");
    CameraEffectTargets targets;
    for (unsigned i = 0; i < VehicleStorage::physical_slots; ++i)
    {
        targets.vehicles[i] = &s.vehicles.physics[i];
        targets.active[i] = static_cast<std::uint32_t>(s.vehicles.race[i].active);
    }
    camera_select_mode(s.controls.controllers[s.controls.current].camera, 10, camera_vehicle_reset_adapter(targets));
}
} // namespace sgn::retail
