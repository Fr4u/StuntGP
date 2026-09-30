#pragma once
#include "sgn/source_camera_spline.hpp"
#include "sgn/retail_camera_initialization.hpp"
#include <span>

namespace sgn::retail
{
// Actual428520/428460 on a captured PMD descriptor, not a SplineView copy.
// Reached XYZ reads borrow the same live allocation. The source does not define
// output W; this API deliberately exposes only its defined XYZ result.
void sample_source_camera_path_428520(const SourceCameraSplineRef &, float, Vec3 &);
// Actual402B90. Keyboard span aliases62D480; speed/parameter are499F88/8C.
// The first camera position is published before the second sample can fail.
void follow_source_camera_path_402b90(contact::player::Controls &, std::array<CameraPresentationState, 4> &,
                                      CameraSessionState &, std::span<const std::uint8_t, 256> keyboard);
// Actual402E90 on the platform's sole controls, session, PMD lookup, matrix
// arena and frame projection. Does not start a race or acquire native inputs.
void prepare_source_camera_view_402e90(const std::shared_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
