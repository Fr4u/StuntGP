#pragma once
#include "sgn/source_camera_mode7.hpp"
namespace sgn::retail
{
// Full4215B0 reached graph, including420F80/420F00/420C40 and mode2 face
// children420A70/4207A0/420690/41FF30. Borrows actual624480 and query owners.
std::uint32_t source_camera_ground_query_4215b0(RaceSessionPlatform &);
class SourceCameraGroundQuery final : public SourceCameraMode7Contact
{
  public:
    std::uint32_t cast_source_track_particle_sphere_4215b0(RaceSessionPlatform &) override;
};
} // namespace sgn::retail
