#pragma once
// NEW HOST diagnostics (not recovered behavior). Inactive unless the process
// environment names an output file in SGN_PROFILE. Scopes accumulate wall time
// per stage on the game thread; the report is rewritten about every 2 s (one
// window) and each window is also appended to <SGN_PROFILE>.history.
//
// NEW HOST (R30) frame-time analysis, all of it only while SGN_PROFILE is set:
//  - every span/value is also logged in a ring of timed events (stage, end,
//    duration, nesting depth), the per-frame scope durations;
//  - two interval series: `present` (one mark per interpolated present, at its
//    pose sample) and `race_pass` (frame_boundary, one per source race pass).
//    Per window: p50/p90/p99/p99.9 interval, 1% and 0.1% low fps (the average
//    rate over the slowest 1% / 0.1% intervals), and the intervals above 1.5x
//    and 2x the median;
//  - SPIKE LOG: the 10 longest intervals of each series with the breakdown of
//    every scope/value that ended inside that interval (time clipped to it);
//    the report lists those above 2x the window median;
//  - the 10 longest spans of the tracked stages (host.source_pass,
//    pass.update_442810, pass.draw_4427b0, host.interp_present) with the
//    breakdown of their nested scopes;
//  - state transitions (presenter activated/deactivated + reason, pacing mode,
//    scene) and persistent info lines (scheduling setup).
// The report is formatted on the game thread (measured as profiler.flush) and
// written by a background thread.
#include <chrono>
#include <cstdint>

namespace sgn::host_profile
{
bool enabled() noexcept;
// NEW HOST (R34): a Scope times itself while the profiler is enabled or the frame
// trace wants this nesting depth (host_frame_trace.hpp: SGN_TRACE / stutter detector).
bool scopes_enabled() noexcept;
// A duration in nanoseconds measured elsewhere (e.g. a GPU timestamp) or a
// counter (0). Accumulated per stage and logged as an instant event.
void add(const char *stage, std::int64_t nanoseconds) noexcept;
// NEW HOST (R30): a plain quantity (vertices, bytes, allocations), reported as
// calls/avg/max/total in its own table.
void sample(const char *stage, std::int64_t value) noexcept;
// NEW HOST (R30): a wall-time span at nesting depth `depth` (see Scope).
void span(const char *stage, std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end,
          unsigned depth) noexcept;
unsigned enter() noexcept; // returns the calling thread's nesting depth, then increments it
void leave() noexcept;
void frame_boundary() noexcept; // once per presented application pass (race_pass series)
// NEW HOST (R33b): source ticks run (442838 loop iterations), reported as sim.tick_rate in the header.
void ticks(std::uint32_t count) noexcept;
// NEW HOST (R30) interval series.
enum class Series : unsigned
{
    present,
    race_pass
};
void mark(Series) noexcept;       // one interval ends here (present: called by the presenter)
void mark_break(Series) noexcept; // the next mark starts a new run (no interval across an inactive period)
// NEW HOST (R30): state change log for the current window; `detail` is copied.
void transition(const char *what, const char *detail = nullptr) noexcept;
// NEW HOST (R30): persistent line in every report header; both strings are copied.
void info(const char *key, const char *text) noexcept;
class Scope
{
    const char *stage_;
    std::chrono::steady_clock::time_point start_;
    unsigned depth_{};
    bool active_;

  public:
    explicit Scope(const char *stage) noexcept : stage_(stage), active_(scopes_enabled())
    {
        if (active_)
        {
            depth_ = enter();
            start_ = std::chrono::steady_clock::now();
        }
    }
    ~Scope()
    {
        if (active_)
        {
            const auto end = std::chrono::steady_clock::now();
            leave();
            span(stage_, start_, end, depth_);
        }
    }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
};
} // namespace sgn::host_profile
#define SGN_PROFILE_CONCAT2(a, b) a##b
#define SGN_PROFILE_CONCAT(a, b) SGN_PROFILE_CONCAT2(a, b)
#define SGN_PROFILE_SCOPE(name) ::sgn::host_profile::Scope SGN_PROFILE_CONCAT(sgn_profile_scope_, __LINE__)(name)
