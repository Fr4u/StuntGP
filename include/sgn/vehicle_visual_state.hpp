#pragma once
#include "sgn/vehicle_math.hpp"

namespace sgn::retail
{
// One visual owner, shared by 00414370, playback and the renderer.
// Angles: vehicle +2070/+2074, stride 98; rotation: +274c, stride 40.
struct VehicleWheelAnimation
{
    float previous_angle{}, current_angle{};
    VehicleMatrix rotation{};
};
} // namespace sgn::retail
