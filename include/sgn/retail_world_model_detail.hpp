#pragma once
#include "sgn/vehicle_math.hpp"
#include "sgn/retail_camera.hpp"

namespace sgn::retail
{
// Full42B620. Current+5C and previous+64 are the caller's actual model owners;
// source previous is published before the new distance/quality selection.
void select_world_model_detail(Vec3 position, const CameraState &, CameraVector forward, std::uint32_t rate_selector,
                               std::uint32_t &current, std::uint32_t &previous);
} // namespace sgn::retail
