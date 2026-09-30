#pragma once
#include <cstdint>
namespace sgn::retail
{
class RaceSessionPlatform;
class SourceCameraMode7Contact;
// Same recovered children shared by404450's branches. Invocation-only owners.
void track_source_camera_target_403770(RaceSessionPlatform &, std::uint32_t vehicle);
void translate_source_camera_403340(RaceSessionPlatform &);
void move_source_camera_403460(RaceSessionPlatform &, SourceCameraMode7Contact &);
} // namespace sgn::retail
