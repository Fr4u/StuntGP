#pragma once
#include "sgn/retail_light_effects.hpp"

namespace sgn::retail
{
struct SourceTrailVertex
{
    CameraVector position;
    float u{}, v{};
    std::uint32_t color{};
};
struct SourceTrailPolygon
{
    CameraVector position, scale;
    std::uint32_t texture{}, flags{}, alpha{}, vertex_count{};
    std::array<SourceTrailVertex, 4> vertices;
};
static_assert(sizeof(SourceTrailVertex) == 28 && sizeof(SourceTrailPolygon) == 160);
static_assert(offsetof(SourceTrailPolygon, vertices) == 0x30);
struct TrailPacketAudit
{
    std::array<bool, 4> staging_w{};
    std::array<bool, 2048> packet_w{};
};
// Complete4342E0 for this caller's0..4 vertices, no culling. Same sprite pool.
bool append_trail_quad_packet(SourceSpritePacketStorage &, const SourceTrailPolygon &,
                              TrailPacketAudit *audit = nullptr);
// Complete4343F0, including actual4280A0. Distinct per-corner colors and
// direct camera+50 rotation; leaves shared sprite basis scratch unchanged.
bool append_trail_billboard_packet(SourceSpritePacketStorage &, const SourceTrailPolygon &, LightCameraView);
} // namespace sgn::retail
