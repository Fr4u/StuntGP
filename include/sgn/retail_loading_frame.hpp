#pragma once
#include "sgn/retail_loading_quads.hpp"

namespace sgn::retail
{
// Separate external interface calls made by432A60/433EE0/433F20/423430.
// Pinned instructions plus Windows SDK surface vtable identify Flip/Unlock;
// see reports/vehicle-work/series6/loading-frame-api-contract-v1. Successful
// calls are mandatory; no default no-op implementation.
struct LoadingFrameDevice
{
    virtual ~LoadingFrameDevice() = default;
    virtual std::uint32_t surface_operation_432a60(std::uint32_t,
                                                   std::uint32_t) = 0; // 61C368 Flip(NULL,DDFLIP_WAIT), slot2C
    virtual std::uint32_t begin_scene() = 0;                           // 5DA1C4 slot24
    virtual std::uint32_t end_scene() = 0;                             // 5DA1C4 slot28
    virtual std::uint32_t finalize_423430(std::uint32_t) = 0;          //(*61C358) Unlock(NULL), slot80 through422BA0
};
struct LoadingFrameWorld
{
    LoadingQuadWorld quads;
    PolygonSubmissionState &submission;    // sole477CCC/59205C/592060/592064 state
    const std::uint32_t &depth_capability; // 5D91B4+64
    const std::uint32_t &fog_a;            // 592054
    const std::uint32_t &fog_b;            // 592058
    std::uint32_t &scene_active;           // 5DA1C8
    std::uint32_t &finalized;              // 6244EC:1 means surface unlocked,0 means locked
    std::uint32_t &render_counter;         // 612984
};
// Full successful4458B0 API path. Source4328C0 leaves all four UV words in
// each untextured stack vertex undefined. This host port initializes those64
// bytes to zero; only its96 defined source bytes admit equality claims.
void clear_loading_frame_4328c0(LoadingFrameWorld, std::uint32_t color, PolygonSubmissionSink &, LoadingFrameDevice &);
void draw_loading_frame_4458b0(LoadingFrameWorld, PolygonSubmissionSink &, LoadingFrameDevice &);
// Shared source API helpers, extracted without changing their bodies/order.
void prepare_loading_surface_432a60(LoadingFrameDevice &);
void begin_loading_scene_433ee0(std::uint32_t &scene_active, LoadingFrameDevice &);
void end_loading_scene_433f20(std::uint32_t &scene_active, LoadingFrameDevice &);
void finalize_loading_frame_431a80(std::uint32_t &finalized, LoadingFrameDevice &);
} // namespace sgn::retail
