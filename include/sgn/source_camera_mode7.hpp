#pragma once
#include <cstdint>
namespace sgn::retail
{
class RaceSessionPlatform;
// Required reached child. Implement4215B0 on this platform's actual PMD/query
// owners; returning an invented miss or decoding a preview TrackGeometry is
// not an implementation. No fallback provider is installed by this packet.
class SourceCameraMode7Contact
{
  public:
    virtual ~SourceCameraMode7Contact() = default;
    virtual std::uint32_t cast_source_track_particle_sphere_4215b0(RaceSessionPlatform &) = 0;
};
// 404450's common previous-target copy and complete mode7 branch404FAD..4051B9.
// Uses actual controls/vehicle/CAM/gravity/query owners. Source4215B0 remains a
// mandatory child only when403460 reaches its collision path. A null service
// throws at403597 after the preceding real query stores; bypass never calls it.
void advance_source_camera_mode7_404fad(RaceSessionPlatform &, SourceCameraMode7Contact *);
} // namespace sgn::retail
