#pragma once
#include "sgn/retail_trail_profiles.hpp"
#include "sgn/retail_trail_packets.hpp"
#include "sgn/retail_screen_polygons.hpp"
#include "sgn/impact_particle_lifetime.hpp"

namespace sgn::retail
{
struct TrailRenderAudit
{
    bool staging_position_w{};
    TrailPacketAudit packet;
    std::span<ScreenPolygonAudit> screen;
};
struct TrailRenderBindings
{
    const TrailProfileBank &profiles;
    SourceTrailPolygon &staging; // sole571330, zero-initialized PE BSS
    SourceSpritePacketStorage &sprites;
    SourceScreenPolygonQueue screen; // borrowed live reader; no eager base lookup
    const ViewportState &viewport;
    LightCameraView camera;
    const VehicleMatrix &current_vehicle_orientation; // global4D5BB8->1BCC, not per-particle basis
    std::uint32_t split_setting;
    TrailRenderAudit *audit{};
};
// Full41F2F0 and its world/screen trail producers. Source ring remains borrowed.
void append_retail_trail(TrailRenderBindings, const TrailRenderProfile &, std::span<const VehicleVector>,
                         std::uint32_t cursor, std::uint32_t count, float scale, float alpha,
                         std::span<const bool> ring_w = {});
// Full416050 visits the already published32-slot list backwards, including
// expired particles. It does not run lifetime or change either input owner.
void append_impact_particle_trails(TrailRenderBindings, const contact::effects::ImpactParticles &,
                                   const contact::effects::ImpactParticleDrawList &,
                                   const contact::effects::ImpactParticleLifetimeAudit *lifetime_audit = nullptr);
} // namespace sgn::retail
