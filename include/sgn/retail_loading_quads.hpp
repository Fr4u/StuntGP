#pragma once
#include "sgn/retail_polygon_submission.hpp"
#include "sgn/retail_hud_lifetime.hpp"

namespace sgn::retail
{
struct LoadingQuadWorld
{
    std::span<std::uint32_t, 35> descriptor;              // 576C38..576CC3, retained shared owner
    std::span<PolygonSubmissionVertex, 4> vertices;       // 58D818, same scratch as other render routes
    std::uint32_t &selected_material;                     // 61C5E4
    std::span<const PolygonSubmissionMaterial> materials; // 6227D4 source texture identities
    const ViewportState &viewport;
    const contact::player::Controls &controls;
    std::span<const ControllerOverlayTransition, 4> overlays;
    const std::uint32_t &tile_material_base; // 61B628
    const std::uint32_t &loading_fade;       // 61B630
    const std::uint32_t &pause_a;            // 61294C
    const std::uint32_t &pause_b;            // 612958
};
// Full named parents and actual432E00/4251A0/432C00/432190 callees in their
// loading domain: rectangle flags2, finite positive dimensions, no rotation.
// Uses existing viewport/material/scratch owners and required API draw sink.
void draw_loading_tiles_445800(LoadingQuadWorld, PolygonSubmissionSink &);
void draw_loading_overlay_4428a0(LoadingQuadWorld, PolygonSubmissionSink &);
void draw_loading_overlay_before_442a00(LoadingQuadWorld, PolygonSubmissionSink &);
void draw_loading_overlay_after_442a20(LoadingQuadWorld, PolygonSubmissionSink &);
} // namespace sgn::retail
