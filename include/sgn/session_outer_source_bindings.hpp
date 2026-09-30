#pragma once
#include "sgn/session_outer_source_state.hpp"
#include "sgn/session_outer_frame.hpp"
#include "sgn/retail_device_setup.hpp"

namespace sgn::retail
{
struct RaceSessionState;

// Required borrow of the selected platform descriptor's source +64 word.
// There is no PE scalar/default for a selected device capability. Original
// 42F420 derives 1/2 from the enumerated descriptor and can replace it with0
// when Z-format enumeration yields no accepted records. A modern backend must
// supply its qualified capability value explicitly; this view does not infer it.
// Caller owns the lvalue and retains it through every consumer invocation.
struct SelectedDeviceDepthBorrow final
{
    const std::uint32_t &word_64;
    explicit SelectedDeviceDepthBorrow(const std::uint32_t &word) noexcept : word_64(word) {}
    SelectedDeviceDepthBorrow(std::uint32_t &&) = delete;
    SelectedDeviceDepthBorrow(const std::uint32_t &&) = delete;
};

// Pure wiring, not initialization. Caller already holds the Session's owner
// thread and retains state/platform capability ownership. No copied flags,
// pointer snapshots, allocations, source reads, device calls or reset here.
SessionOuterDeviceWords session_outer_device_words(RaceSessionState &) noexcept;
DemoTailWords session_outer_demo_words(RaceSessionState &) noexcept;
DeviceSetupGlobals session_outer_device_setup_globals(RaceSessionState &) noexcept;
SessionPolygonCapabilities session_outer_polygon_capabilities(RaceSessionState &, SelectedDeviceDepthBorrow) noexcept;

// Complete42F300..42F339 (58 bytes/13 instructions). The four represented
// submission words stay in the EXISTING PolygonSubmissionState. Other seven
// words stay in the new missing-word owner. These owners must be distinct.
// Does not reset477CCC,6244F0,scene/finalized,counter,descriptor,cache,vertices,
// indices,capabilities or API device state. Invoke only at an explicit source
// 42F300 boundary; a borrow/Renderer rebind is not such a boundary.
void reset_device_source_globals_42f300(SessionOuterDeviceSourceState &, PolygonSubmissionState &) noexcept;
} // namespace sgn::retail
