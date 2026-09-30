#pragma once
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/retail_track_geometry_animation.hpp"
#include "sgn/retail_camera_initialization.hpp"

namespace sgn::retail
{
// Full42AC70 including its real426ED0 geometry/particle effects. The caller
// first binds descriptor0C to the actual background command's retained pose.
// Existing native caller admits background0 as the actual global track.
// Publish61C52C and captured61C530 separately; later reads never derive the
// part publication from61C52C. Source PMD rendering remains a separate boundary.
// Candidate pending full original/native composition proof.
void produce_world_track_base(ModelResource &, const CameraState &, const CameraPresentationState &, CameraLifecycle &,
                              const ViewMatrixArena &, const CameraMatrix &shared_identity,
                              const ProjectionParameters &, WorldPolygonFrame &, std::span<TrackPartAnimation>,
                              TrackNormalView, TrackGeometryAnimationEffects, TrackGeometryAnimationAudit = {});
} // namespace sgn::retail
