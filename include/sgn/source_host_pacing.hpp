#pragma once
#include "sgn/source_application_entry.hpp"
#include "sgn/retail_frontend_clock.hpp"
#include "sgn/retail_tick_clock.hpp"
#include <cstdint>
#include <memory>

namespace sgn
{
class Renderer;
}
namespace sgn::host_presentation
{
class InterpolatingPresenter;
}

namespace sgn::retail
{
// NEW HOST pacing (R18 "P2"), not recovered code. Read-only borrows of the
// single source world that 44EA60 itself reads. Nothing here is written.
struct SourceHostPacingWorld
{
    const std::uint32_t &scene;              // 612BE8, read by 44EB1B
    const RetailTickClock &clock;            // 612D00/04/08
    const std::uint32_t &fixed_mode_a;       // 60F2AC (DISABLE_FRAMERATE_CORRECTION)
    const std::uint32_t &fixed_mode_b;       // 60DF9C
    const std::uint32_t &window_word_61c374; // read by 44EB09
    const std::uint32_t &window_word_61c364; // read by 44EB12
};

// Decorates the native PeekMessageA adapter at the 44EAE6 boundary. 44EA60's
// body and order stay untouched: when the queue is empty the original falls
// through to 44E9D0 and, if the window words allow it, one full scene pass
// (442810+4427B0 or 441F00+441D70) and 44E6B0. This adapter only decides WHEN
// "queue empty" is reported:
//  - scene 1/2 (source clock active): only once the unchanged 442490/442834
//    (441F1F) arithmetic would enter at least one tick. Until then it waits on
//    a high-resolution timer and the message queue (no busy spin). So every
//    pass contains >= 1 tick and each per-pass child (45F210/40DAD0 previous
//    key copy, 4023F0, 446110/462280 audio, 4427B0/441D70 draw incl. the
//    42DCF0 light phases, 432A60 Present, 44E6B0) runs at most once per 60 Hz
//    tick: the cadence of the original on a 60 Hz display, at any refresh
//    rate and with vsync on or off.
//  - fixed mode (60F2AC or 60DF9C == 1: exactly one tick per pass by 4424DE)
//    or any other scene: passes follow a host 60 Hz deadline in the same
//    counter units (clock.increment), so the game speed stays display-free.
//  - window inactive/iconic (44EB10/44EB19 skip the pass): blocks on the
//    message queue with a 100 ms bound and reports "empty" once per wake so
//    44E9D0 still runs; the original spun here.
//  - before a pass is released: waits on the swap chain's frame-latency
//    object (SetMaximumFrameLatency(2), renderer.cpp) together with the
//    message queue, before 45F210 samples input. One wait is held as a credit
//    until a Present is observed, so a pass without 432A60 Present never
//    unbalances the latency semaphore.
// No tick is created, skipped, scaled or re-timed; 442490 still reads the
// counter itself and runs the unchanged catch-up loop.
//
// NEW HOST high-refresh presentation (interp R19): with a presenter bound and
// active (race scene, canonical frames tagged), the time between two due
// ticks is no longer idle: whenever the swap chain's frame-latency object
// grants a display slot (vsync interval 1 at any refresh rate, or at once when
// uncapped/tearing), the presenter shows the state interpolated between the
// last two canonical frames. This happens here, between passes; 44EA60 sees
// "queue empty" only for a due tick, exactly as before, so no source function
// (442810, 45F210/40DAD0 input, 4023F0, 446110/462280 audio, 4427B0 incl.
// 42DCF0/425AD0/44FD60/61C508/612984, 44E6B0) runs more often than once per
// tick and input edges accumulate between ticks as at 60 Hz.
//
// NEW HOST (R27) frame rate limiter (host_graphics::Settings::frame_rate_limit,
// 0 = off, 30..1000 fps, applied live via revision()). It caps only these
// interpolated presents: after the presenter has its back buffer and before it
// samples the display time (InterpolatingPresenter PresentGate), the adapter
// waits for the next slot (period 1/limit, QPC) with the high-resolution
// waitable timer and spins the last ~0.5 ms. Slots keep their phase while on
// time; a late present restarts the phase one full period after itself, so two
// presents are never closer than the period (VRR: limit = refresh - 3 keeps the
// display inside its range with vsync off). A slot that could not finish its
// Present before the due tick (estimate: EMA of sample->Present return) is not
// taken: the adapter waits for the tick, the pass runs on time and the present
// follows it. Input arriving during the wait is dispatched first (the slot is
// absolute). With interpolation off (or outside the race) the source 60 Hz pass
// is the only frame source and is never limited: limits below 60 are ignored.
//
// NEW HOST (R28b) the same tick guard also applies with no limit when vsync is
// off: a present whose estimated back-buffer wait + build + Present would run past
// the due tick is not started (the pass runs first, the present follows). The
// estimates rise at once on a slow frame and decay slowly. Uncapped mode also gets
// an implicit cap of 2x the display refresh (tearing or exclusive fullscreen) or
// 1x (windowed without tearing, where DWM shows at most one frame per refresh);
// with an unknown refresh only the guard applies. VSync on without a limit is
// unchanged: display slots pace it and skipping one would cost a refresh.
//
// NEW HOST (R30) frame-time consistency (timing only; no tick, source value or
// pixel of a frame changes):
//  - Tick due times are exact (the rebased counter's fractional value): a tick
//    wait no longer sleeps up to one 6000 Hz unit (167 us) past the due time and
//    the tick guard no longer uses a one-unit-early estimate. Tick waits that
//    decide a displayed frame's time (presenter inactive, vsync off) end with a
//    short spin; its window adapts to the measured timer wake-up lateness
//    (150 us..1 ms). The limiter spin uses the same window.
//  - With the presenter active the race Present is suppressed, so the pass no
//    longer waits for a frame-latency slot first (it presents nothing).
//  - Present-then-pass (vsync off or a binding limit): a pass may start up to a
//    bounded budget after its due tick (at most 4 ms and never past half a tick
//    minus the measured pass cost, the margin by which the presenter needs that
//    frame). A present whose slot fits before due + budget is made first, the pass
//    follows directly: the gap around every pass is one present + one pass, not
//    an idle guard window + a pass. The simulation is unaffected (442490 still
//    runs every due tick, with the same counter reads).
//  - VSync on with a limit at or above the refresh: the limit cannot bind, so
//    the display slots pace (as vsync without a limit); before R30 the limiter
//    beat against the display (240 vs 239.76 Hz) and its tick guard dropped a
//    display slot each tick.
//  - VSync slots: each present samples the state at its display slot (the time
//    the frame-latency object was signalled, or the next slot of the refresh
//    grid when the slot was already free), not at the moment it was built, so a
//    present delayed by a pass shows the pose of the refresh it is shown on.
//  - Uncapped (vsync off, no limit) with tearing: the 2x refresh cap drops to 1x
//    while more than a quarter of the presents miss their slot, and returns after
//    2 s when present cost and GPU time fit the 2x period.
//
// NEW HOST (R31) cooperative presentation inside the pass: while a pass that
// was released with the presenter active runs, the race draw's yield points
// (host_presentation::pass_yield_point) let this adapter present one
// interpolated frame whenever its display slot is due (polled, never waited
// for), so a 5-9 ms source pass no longer leaves the display without frames.
// The frame-latency setting is left as chosen: yields are at most ~2.3 ms
// apart, less than one refresh even at 240 Hz, so one queued frame suffices.
// Counters interp.present_inside_pass, pacing.yield_checks; scope
// host.pass_yield_present (nested inside the draw.* scope of its yield point).
// Profiler (SGN_PROFILE): span host.source_pass (released pass until the next
// peek), transitions pacing.mode/pacing.scene/pacing.implicit_cap, samples
// pacing.pass_start_late_us, sched.timer_wake_late_us, sched.spin_window_us.
class SourceHostPacedMessages final : public SourceApplicationMessages
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    SourceHostPacedMessages(SourceApplicationMessages &native, SourceHostPacingWorld world,
                            FrontendPerformanceCounter &counter, const std::unique_ptr<Renderer> &renderer_slot,
                            host_presentation::InterpolatingPresenter *presenter = nullptr);
    ~SourceHostPacedMessages() override;
    SourceHostPacedMessages(const SourceHostPacedMessages &) = delete;
    SourceHostPacedMessages &operator=(const SourceHostPacedMessages &) = delete;
    bool peek_removed() override;
    std::uint32_t message_id() const override;
    std::uint32_t source_wparam() const override;
    void translate() override;
    void dispatch() override;
};

// NEW HOST pacing (R18 P2) for the 445940 loading sequence, not recovered code.
// 445940 runs a fixed count of display frames (445999..4459CF: frames+2
// iterations of 4459A1 CALL 4458F0 update + 4459A6 CALL 4458B0 draw/Present,
// fade-out armed at 4459AB count 0x78) inside one outer pass, so its length followed the display
// rate (2 s at 60 Hz, 0.85 s at 144 Hz, ~0 uncapped) while its 442A40 fades
// advance per tick in 442BE0. Bound only to RetailLoadingPresentation: the
// single 442490 read in each 4458F0 iteration (4424A8 CALL 434820) returns
// once the unchanged 442490/loop test would enter at least one tick, so each
// loading frame again carries >= 1 tick. The fixed-mode branch (4424DE) never
// reads the counter and is unaffected. Frequency and values are the inner
// counter's; nothing is scaled or stored.
class SourceHostLoadingPacedCounter final : public FrontendPerformanceCounter
{
    FrontendPerformanceCounter &inner_;
    const RetailTickClock &clock_;
    const std::uint32_t &fixed_mode_a_;
    const std::uint32_t &fixed_mode_b_;
    void *timer_{};

  public:
    SourceHostLoadingPacedCounter(FrontendPerformanceCounter &inner, const RetailTickClock &clock,
                                  const std::uint32_t &fixed_mode_a, const std::uint32_t &fixed_mode_b);
    ~SourceHostLoadingPacedCounter() override;
    SourceHostLoadingPacedCounter(const SourceHostLoadingPacedCounter &) = delete;
    SourceHostLoadingPacedCounter &operator=(const SourceHostLoadingPacedCounter &) = delete;
    std::uint64_t counter() override;
    std::uint64_t frequency() override;
};

// NEW HOST (R30) scheduling of the game thread, which runs the source passes,
// the renderer and the pacing (the thread that constructs this object), and of
// the process, for the object's lifetime. Every step is optional and reported
// (SGN_PROFILE info lines, OutputDebugString); a step Windows refuses is skipped.
//  - timeBeginPeriod(1): 1 ms system timer (millisecond timeouts, Sleep).
//  - process and thread power throttling off (EcoQoS / execution speed), and
//    PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION opted out (Windows 11:
//    timer resolution kept while the window is occluded or minimized).
//  - THREAD_PRIORITY_ABOVE_NORMAL, then MMCSS task "Games".
// SGN_SCHED=0: none of it (A/B measurement); SGN_SCHED=2: everything but MMCSS.
class SourceHostScheduling final
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    SourceHostScheduling() noexcept;
    ~SourceHostScheduling();
    SourceHostScheduling(const SourceHostScheduling &) = delete;
    SourceHostScheduling &operator=(const SourceHostScheduling &) = delete;
};

// Pure predicate shared with the adapter (host read, no store): counter units
// until 442490 followed by the 442834/441F1F test enters a tick; 0 = due now.
// Mirrors 4424AD..4424D4 (|observed-scheduled| > 30*increment resyncs to
// scheduled=observed-1, one iteration) and the unsigned JNC at 442836/441F21.
std::uint32_t source_host_counts_until_tick(const RetailTickClock &clock, std::uint32_t observed) noexcept;
} // namespace sgn::retail
