#pragma once
#include "sgn/retail_polygon_submission.hpp"

namespace sgn::retail
{
struct QuadSubmission
{
    std::span<const std::uint32_t, 35> descriptor;        // 576C38, copied locally by432E00
    std::span<PolygonSubmissionVertex, 4> vertices;       // same retained58D818 scratch
    std::uint32_t &selected_material;                     // same61C5E4 as loading/font/screen submission
    std::span<const PolygonSubmissionMaterial> materials; // live6227D4 namespace
    const ViewportState &viewport;
};
// Full source helpers in the finite arithmetic domain. Clip changes its input,
// including pivot writes before a rejection. Vertex helpers retain UV1 words.
bool clip_quad_4251a0(std::span<std::uint32_t, 35>, const ViewportState &);
void build_explicit_quad_432af0(std::span<const std::uint32_t, 35>, std::span<PolygonSubmissionVertex, 4>);
void build_rectangle_quad_432c00(std::span<const std::uint32_t, 35>, std::span<PolygonSubmissionVertex, 4>);
// Full432E00, including actual blend432190. Bit40 bypasses clipping,bit4
// selects explicit corners,bit1 rotates,bit8 scales,bit20 supplies diffuse.
// These are descriptor flags; the separate descriptor[11] is the blend mode.
// Borrowed owners must be valid/nonoverlapping. Rotation uses the existing
// recovered finite angle-index domain. Missing accessed materials throw a host
// guard. Ignored HRESULTs stay ignored; no per-call scratch/cache clearing.
void submit_quad_432e00(QuadSubmission, PolygonSubmissionSink &);
} // namespace sgn::retail
