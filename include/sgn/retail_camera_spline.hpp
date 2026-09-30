#pragma once
#include "sgn/retail_camera.hpp"
#include "sgn/camera_path.hpp"

namespace sgn::retail
{
// 404450's mode7 branch404FAD..4051B9, including its common previous-target
// copy and403460/403340 tail.499F8C is shared CameraSessionState.path_parameter.
// 428520 does not define output W. Native code retains camera position W;
// source parity is claimed for defined XYZ and the other represented fields.
void camera_spline_chase_tick(CameraState &, const CameraVehicle &, float &path_parameter, SplineView,
                              CameraVector gravity, CameraCollision);
} // namespace sgn::retail
