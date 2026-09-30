#pragma once
#include "sgn/retail_polygon_submission.hpp"

namespace sgn::retail
{
// One persistent owner. delta=592018/1C/20, second=592030/34/38,
// first=592040/44/48. The intervening source words are not part of this owner.
struct UiLineClipScratch
{
    std::array<std::uint32_t, 9> words{};
};
static_assert(sizeof(UiLineClipScratch) == 36);
struct UiLineEndpoints
{
    std::array<float, 3> first{}, second{};
};
static_assert(sizeof(UiLineEndpoints) == 24);
struct UiLineSubmission
{
    UiLineClipScratch &clip;
    std::span<PolygonSubmissionVertex> alpha_vertices; // same58D818, first2 on a draw
    const ViewportState &viewport;
    const std::uint32_t &specular_enabled;  // same592050; source leaves it alone
    const std::uint32_t &selected_material; // same61C5E4; NOT invalidated by null binding
};
// Complete original helpers, finite input/result arithmetic domain. Raw unused
// scratch words may contain any bits. Source X retains the unrounded product;
// Y/Z products round to binary32 before endpoint arithmetic.
void advance_ui_line_first_431aa0(UiLineClipScratch &, float fraction);
void advance_ui_line_second_431b40(UiLineClipScratch &, float fraction);
// Rejection still publishes both endpoints to scratch, retaining old deltas.
// Input endpoints are modified only on source success. No viewport repair.
bool clip_ui_line_431be0(UiLineEndpoints &, UiLineClipScratch &, const ViewportState &);
// Complete pixel-space431F10 and virtual640x480 adapter447840. Arguments are
// source stack-local copies; shared scratch survives rejection and later calls.
// Host guards reject nonfinite accessed arithmetic and overlapping owners;
// a visible draw needs at least2 alpha vertices. Guards precede publication.
// HRESULTs remain ignored. The sink must consume/copy borrowed draw data before
// returning. These functions perform no source-clock or monitor operation.
void submit_ui_line_431f10(UiLineSubmission, UiLineEndpoints, std::uint32_t color, PolygonSubmissionSink &);
void submit_ui_line_447840(UiLineSubmission, UiLineEndpoints, std::uint32_t color, PolygonSubmissionSink &);
} // namespace sgn::retail
