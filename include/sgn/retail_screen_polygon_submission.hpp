#pragma once
#include "sgn/retail_polygon_submission.hpp"
#include "sgn/retail_screen_polygons.hpp"

namespace sgn::retail
{
struct ScreenPolygonSubmission
{
    // Full retained owner, including the next slot read by43306C after the loop.
    ConstScreenPolygonBaseReader base;                    // read once at432F7E, after first sink call
    std::uint32_t &count;                                 // 61C604, same owner as SourceScreenPolygonQueue
    std::uint32_t &selected_material;                     // 61C5E4, shared with LoadingQuadWorld
    std::span<const PolygonSubmissionMaterial> materials; // live6227D4 namespace
    // Source uses a local eight-vertex FVF2C4 array. Host storage is initialized
    // and borrowed: unwritten UV1 retains opaque host bits, not recovered values.
    std::span<PolygonSubmissionVertex, 8> scratch;
    const ViewportState &viewport;
};
void apply_screen_polygon_blend_432190(std::uint32_t mode, PolygonSubmissionSink &);
void reset_screen_polygon_count_425870(std::uint32_t &count);
void reset_screen_material_cache_42e420(std::uint32_t &selected_material);
// Full432F60, actual432190 blend and425870 reset. Healthy input corner count
// 0..8; slots must own the next record read even at count zero. Bounds failures
// throw FormatError; ignored source HRESULTs remain ignored. No implicit cache
// reset, coordinate transform, clipping, alpha mask or untouched-slot clearing.
// Host diagnostics from the already-required source reads; not retail EAX.
struct ScreenPolygonSubmissionResult
{
    std::uint32_t entry_count{}, submitted{};
};
ScreenPolygonSubmissionResult submit_screen_polygons_432f60(ScreenPolygonSubmission, PolygonSubmissionSink &);
} // namespace sgn::retail
