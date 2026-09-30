#pragma once
#include "sgn/source_outer_control.hpp"
#include "sgn/retail_frontend_clock.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/session_outer_capture.hpp"
#include "sgn/source_input_dispatch.hpp"
#include "sgn/session_window_input.hpp"
#include <vector>

namespace sgn::retail
{
struct SessionWorldTickAudit;
class RaceSessionPlatform;
// Native QPC acquisition only; no elapsed-time accumulator or Hz multiplier.
class Win32SourceOuterCounter final : public FrontendPerformanceCounter
{
  public:
    std::uint64_t counter() override;
    std::uint64_t frequency() override;
};
// Exact existing434840 writes into the one retained world owner. Explicit
// startup call, never a per-frame initialization. This is not full442120.
// The platform binding is valid before Session attachment. It owns no state,
// selects no display cadence, and checks the creating thread before source I/O.
void initialize_platform_source_clock_434840(RaceSessionPlatform &, FrontendPerformanceCounter &);
// Explicit leaf binding only. Borrows the same platform clock and supplied
// counter.445FA0 composition and the separate4420D8 service keep distinct reads.
void reset_platform_source_clock_442100(RaceSessionPlatform &, FrontendPerformanceCounter &);
// Existing Session entry delegates to the same platform binding exactly once.
void initialize_session_source_clock_434840(RaceSession &, FrontendPerformanceCounter &);
// Host trace of complete composition boundaries, NOT all nested source calls.
// The grouped input boundary covers45F210 AND40DAD0; the audio boundary covers
// 446110 AND462280. 'entered' is published before that synchronous operation.
enum class SourceOuterBoundary : std::uint32_t
{
    numeric = 0x429510,
    camera_reset = 0x4023f0,
    input_pair = 0x44281a,
    sample_clock = 0x442490,
    entered_iteration = 0x442838,
    audio_pair = 0x44288c,
    scene_transition = 0x441dc0,
    draw = 0x4427b0,
    capture_seal = 0xff000001
};
struct SourceOuterTrace
{
    std::vector<SourceOuterBoundary> entered;
    std::vector<SceneOperation> missing_scene_operations;
};
// Invocation borrows only. Source world, frontend, clock and input have their
// existing persistent owners. A null transition service performs the owned
// 431810 release and admitted warm children, stopping at the first missing
// audio dependency or42E6E0 after445FA0/42EA80. Full scene initialization remains blocked.
struct SourceRaceOuterContext
{
    RaceSession &session;
    FrontendState &frontend;
    FrontendPerformanceCounter &counter;
    SessionWindowInput &input; // same persistent native owner, checked before source entry
    SceneTransitionServices *transition_services;
    SourceOuterTrace &trace;
    SessionWorldTickAudit *tick_audit{};
};
struct CapturedSourceRaceOuterFrame
{
    SourceOuterOutcome update;
    std::optional<CapturedSessionOuterFrame> frame;
};
// Called only at the original44EB25 race branch. This new member declaration
// is added to SessionOuterCapture by integration.patch. It uses that owner's
// EXISTING cadence_ and texture bank; no second serial or source tick owner.
// Extra monitor refreshes call only present_captured_session_outer_frame.
} // namespace sgn::retail
