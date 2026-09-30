#pragma once
#include "sgn/retail_spline.hpp"

namespace sgn::retail
{
struct CameraPathInput
{
    bool increase_speed{}, decrease_speed{};
};
struct CameraPathSample
{
    Vec3 position;
    bool adjacent_storage_read{};
    // 428460 copies an uninitialized stack W; 428520 cubic writes XYZ only.
    // Neither path establishes a usable fourth component.
};
CameraPathSample camera_sample_path(SplineView, float parameter);          // 00428520 + 00428460
void camera_advance_path(float &parameter, float &speed, CameraPathInput); // 00428740
} // namespace sgn::retail
