#pragma once
#include "sgn/contact_particle_allocator.hpp"
#include "sgn/model_resources.hpp"
#include "sgn/retail_camera.hpp"
#include "sgn/retail_track_normal_allocation.hpp"
#include "sgn/retail_world_globals.hpp"
#include "sgn/track_part_animation.hpp"

namespace sgn::retail
{
// Borrowed canonical owners. Aliased parts must retain the same spans/pose.
struct TrackGeometryAnimationPart
{
    ModelTransform *transform{};
    const Vec3 *pivot{};
    std::uint16_t flags_02{};
    std::span<VehicleVector> vertices;
    std::span<std::array<std::uint32_t, 4>> coordinates;
    std::span<std::array<std::uint8_t, 8>> polygons;
};
struct TrackGeometryAnimationEffects
{
    VehicleRandom &random;
    contact::effects::EffectParticleBank &particles;
    const WorldGravity &gravity;
    std::uint32_t source_rate;           // 61297C, existing source selector, never display Hz
    const ModelTransform &template_pose; // sole mutable4864E8, including live tail references
};
// Optional diagnostic of the write-only math scratch61C3A4. This is not a
// second runtime owner. Stack-undefined normal/forward/up W lanes retain their
// destination bits under an explicit host policy, not a recovered W value.
struct TrackGeometryAnimationAudit
{
    CameraMatrix *rotation_scratch{};
    contact::effects::EffectParticleFrameAudit *particle_frames{};
};
// Full426ED0 and its16-way table, including426A00's real particle allocation.
// Validates host spans before the first store. Does not advance427140 phases.
void animate_track_geometry(std::span<TrackGeometryAnimationPart>, std::span<TrackPartAnimation>, TrackNormalView,
                            TrackGeometryAnimationEffects, TrackGeometryAnimationAudit = {});
// Full descriptor48 (LOD0) binding. No render mesh copies or selected-LOD input.
void animate_track_geometry(ModelResource &, std::span<TrackPartAnimation>, TrackNormalView,
                            TrackGeometryAnimationEffects, TrackGeometryAnimationAudit = {});
} // namespace sgn::retail
