#pragma once
#include "sgn/retail_world_model_lists.hpp"
#include "sgn/retail_world_sprite_producer.hpp"
#include "sgn/retail_world_track_base.hpp"
#include "sgn/retail_world_track_parts.hpp"
#include "sgn/retail_world_background_models.hpp"

namespace sgn::retail
{
// Synchronous borrowed bindings to existing owners. No persistent mirror of
// the Session, RNG, descriptor poses, light buffers or draw queues is created.
struct WorldFrameProduction
{
    ModelResources &models;
    std::size_t track, sky;
    WorldModelDrawState &draws;
    SourceSpritePacketStorage &sprites;
    VehicleStripGeometry &strip; // sole generated571314 descriptor and buffers
    const CameraState &camera;
    const CameraPresentationState &presentation;
    CameraLifecycle &lifecycle;
    ViewMatrixArena &matrices;
    const CameraMatrix &identity;
    const ProjectionParameters &projection;
    WorldPolygonFrame &frame;
    CameraMatrix &frame_world_view; // sole61C594
    std::span<TrackPartAnimation> animations;
    TrackNormalView normals;
    VehicleRandom &random;
    contact::effects::EffectParticleBank &particles;
    const WorldGravity &gravity;
    WorldModelLightingBinding lighting;
    const std::uint32_t &lighting_enabled; // 624980
    std::uint32_t &quality;                // 624984,42ADD0 may clear it
    const std::uint32_t &source_rate;      // 61297C
    float ground_level;
    std::uint32_t scene_mode;
    const SceneResources *registry{}; // actual Session binding; absent only for standalone leaf fixtures
};
// Full42B040, including track effects and final42B740/42B200. It must run once
// in source order after42E800 and prepared lights, before clipping/projection.
// This is authoritative producer work, never a per-display replay function.
// Candidate pending full original/native composition qualification.
void produce_world_frame(WorldFrameProduction, TrackGeometryAnimationAudit = {});
} // namespace sgn::retail
