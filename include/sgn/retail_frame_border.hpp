#pragma once
#include "sgn/retail_polygon_submission.hpp"

namespace sgn::retail
{
struct FrameBorderWorld
{
    const ViewportState &viewport;                  // 576B80/84,576B94/98
    PolygonSubmissionState &submission;             // same592050 and58D818 owner as world/loading
    std::uint32_t &selected_material;               // 61C5E4 shared with all UI consumers
    const std::uint32_t &quality;                   // 624984, read AFTER DrawPrimitive returns
    const std::uint32_t &legacy_driver_restriction; // 589E08, checked on each helper entry
};
// Full431830; normal442630 passes color0. Borrows the FIRST EIGHT persistent
// alpha_vertices (58D818), never allocates or resets their retained UV words.
// Caller provides that minimum extent independently of world-list demand and
// keeps scratch storage stable during sink calls. Missing extent is a host
// failure before source calls/writes. Other submission buffers/globals retain.
// Literal primitive2/FVF2C4/count8/flags14; ignored HRESULTs remain ignored.
// This producer does not supply primitive2 support to the CPU/GPU receiver.
void draw_frame_border_431830(FrameBorderWorld, std::uint32_t color, PolygonSubmissionSink &);
} // namespace sgn::retail
