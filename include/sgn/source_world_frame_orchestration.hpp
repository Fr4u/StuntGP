#pragma once
#include "sgn/source_world_track_view.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/retail_camera_lifecycle.hpp"

namespace sgn::retail
{
// Synchronous borrows of the actual Session owners. There are no native
// model/normal mirrors, fabricated animation records or substitute banks.
struct SourceWorldFrameProduction
{
    ModelResources &models;
    SceneResources &scene;
    WorldModelDrawState &draws;
    SourceSpritePacketStorage &sprites;
    const CameraState &camera;
    const CameraPresentationState &presentation;
    CameraLifecycle &lifecycle;
    ViewMatrixArena &matrices;
    const CameraMatrix &identity;
    const ProjectionParameters &projection;
    WorldPolygonFrame &frame;
    CameraMatrix &frame_world_view;
    SourceTrackAnimationOwners &animations;
    const std::uint32_t &lighting_enabled; // 624980
    std::uint32_t &quality;                // 624984
    const std::uint32_t &rate;             // 61297C, remains live through children
    const std::uint32_t &split_setting;    // 576BE0
    float ground_level;
    std::uint32_t scene_mode;
};
// Complete source sequencing42B040, actual PMD/sprite children and group
// publications, including42A320/42A570 and42B400 on the same platform's
// actual624534 light pointers. Caller must first run42DCF0 at the source
// camera-frame stage. Partial light/color stores are retained on failure.
// Requires the already attached actual level lease; a source-stage exception
// detaches it. Only the completed next resource epoch may attach a new lease.
// This does not qualify the separate clipping/submission or strip path.
void produce_source_world_frame_42b040(SourceWorldFrameProduction);
void produce_source_world_sky_42ad30(const ModelDrawCommand &, SourceWorldFrameProduction);
void produce_source_world_track_parts_42add0(SourceWorldFrameProduction);
void produce_source_world_regular_models_42af90(std::span<const ModelDrawCommand>, SourceWorldFrameProduction);
} // namespace sgn::retail
