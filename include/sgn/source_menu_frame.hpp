#pragma once
#include "sgn/source_menu_update.hpp"
#include "sgn/retail_frontend_clock.hpp"
#include "sgn/retail_loading_frame.hpp"

namespace sgn::retail
{
// These are exact source call boundaries, not optional event notifications.
// Bind complete children or throw at the unresolved operation. In particular,
// 45F210 must NOT append the race-only 40DAD0 input dispatcher.
struct SourceMenuFrameChildren : SourceMenuUpdateChildren, SourceMenuDrawChildren
{
    virtual void acquire_input_45f210(FrontendState &) = 0;
    virtual void update_controls_40d860(FrontendState &) = 0;
    virtual void maintain_audio_462280(FrontendState &) = 0;
    virtual void prepare_camera_402e90(FrontendState &) = 0;
    virtual void prepare_geometry_44fd60(FrontendState &) = 0;
};
// Full 441F00 call/control flow. Uses the platform's sole clock, input arrays
// and counters through FrontendState.world(). No dt or display rate argument.
void update_source_menu_frame_441f00(FrontendState &, FrontendPerformanceCounter &, SourceMenuFrameChildren &,
                                     SceneTransitionServices &);
// Full 441D70 successful-device path. Frame borrows must name this same world's
// rendering owners; device and polygon sink are the application's actual ones.
// The shared 4328C0 helper retains its documented zero-filled undefined UV policy.
void draw_source_menu_frame_441d70(FrontendState &, LoadingFrameWorld, PolygonSubmissionSink &, LoadingFrameDevice &,
                                   SourceMenuFrameChildren &);
} // namespace sgn::retail
