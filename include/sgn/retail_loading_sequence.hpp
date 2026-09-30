#pragma once
#include "sgn/retail_loading_frame.hpp"
#include "sgn/retail_loading_update.hpp"
#include "sgn/retail_lighting.hpp"

namespace sgn::retail
{
struct LoadingSequenceWorld
{
    LoadingPresentationUpdateWorld update;
    LoadingFrameWorld frame;
    std::uint32_t &loading_fade; // same61B630 referenced by frame.quads
    LightFadeState &gamma;
    LightGammaDevice *gamma_device; // null only when source gamma device is absent
};
// 445940 with actual clock/overlay/frame/gamma dependencies. iterations follows
// the original unsigned frames+2 wrap, with no display-frequency scaling or
// invented frame cap. The source callers use300/60/120 and30/15/0.
void run_loading_sequence_445940(LoadingSequenceWorld, std::uint32_t frames, std::uint32_t initial_hold,
                                 std::uint32_t fade_out_hold, FrontendPerformanceCounter &, PolygonSubmissionSink &,
                                 LoadingFrameDevice &);
} // namespace sgn::retail
