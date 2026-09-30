#pragma once
#include "sgn/retail_world_model_producer.hpp"
#include "sgn/retail_camera_initialization.hpp"

namespace sgn::retail
{
// Full428040 dispatch with427CC0 radius/plane and427B40 eight-point paths.
// part=-1 reads descriptor bounds; otherwise reads that selected LOD part.
// Returns0 inside,1 rejected,2 intersecting. Only the eight-point branch
// writes the single retained577A90 matrix. Projection near/far are live+198/19C.
std::uint32_t classify_world_model_visibility(ModelResource &, unsigned source_lod, std::int32_t part,
                                              const ModelTransform &outer, const CameraState &,
                                              const CameraPresentationState &, const CameraLifecycle &,
                                              const ViewMatrixArena &, const CameraMatrix &shared_identity,
                                              const ProjectionParameters &, CameraMatrix &bound_matrix);
// Pure complete427CC0 arithmetic, including the distinct whole-model path.
std::uint32_t classify_world_model_radius(std::uint16_t radius, const VehicleVector &bound_center, bool whole_model,
                                          const ModelTransform &outer, const ModelTransform &part, Vec3 pivot,
                                          const CameraMatrix &part_rotation, Vec3 camera, CameraVector forward,
                                          std::span<const CameraVector, 4> planes, float far);
// Invocation-only field reader. The arithmetic asks for a component only at
// its reached use, so an early rejection does not pre-read later scale fields.
struct WorldModelRadiusPartRead
{
    const void *owner;
    float (*component)(const void *, unsigned byte_offset);
    float read(unsigned offset) const
    {
        return component(owner, offset);
    }
};
// Same arithmetic with only actually consumed part fields. W has no parameter.
std::uint32_t classify_world_model_radius_fields(std::uint16_t radius, const VehicleVector &bound_center,
                                                 bool whole_model, const ModelTransform &outer,
                                                 WorldModelRadiusPartRead part, Vec3 pivot,
                                                 const CameraMatrix &part_rotation, Vec3 camera, CameraVector forward,
                                                 std::span<const CameraVector, 4> planes, float far);
// Complete427B40 numerical loop after its real44FBD0 producer. The caller's
// eight points are actual references from descriptor+40, never generated AABBs.
std::uint32_t classify_world_model_eight_points(std::span<const VehicleVector *const, 8>, const CameraMatrix &,
                                                const ProjectionParameters &, float far);
} // namespace sgn::retail
