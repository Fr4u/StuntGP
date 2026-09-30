#pragma once
#include "sgn/retail_text_submission.hpp"
#include "sgn/retail_quad_submission.hpp"
#include "sgn/retail_ui_line.hpp"

namespace sgn::retail
{
// Live source slots4851E0/4851E4. Resolve each at its own source read boundary.
// The selected text storage must remain valid through its synchronous text call.
struct PauseMeterLabels
{
    virtual ~PauseMeterLabels() = default;
    virtual std::span<const std::uint8_t> read_left_4851e0() = 0;
    virtual std::span<const std::uint8_t> read_right_4851e4() = 0;
};
struct PauseMeterSubmission
{
    TextSubmission text;                               // sole font/material/cache/specular/viewport owners
    std::span<std::uint32_t, 35> descriptor;           // same576C38;13 words retained
    std::span<PolygonSubmissionVertex> alpha_vertices; // same58D818,at least4
    UiLineClipScratch &clip;
    PauseMeterLabels &labels;
};
// Full446D90; uint32 percent is latched, including wrap outside0..100.
// Exact source stores/call order; no clock, font selection, cache repair or reset.
// Finite accessed arithmetic and child nonoverlap/lifetime domains are required.
// Host guard failures do not roll back earlier completed children.
void submit_pause_meter_446d90(PauseMeterSubmission, std::uint32_t percent, float x, float y, float width, float height,
                               PolygonSubmissionSink &);
} // namespace sgn::retail
