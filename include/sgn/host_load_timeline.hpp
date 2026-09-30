#pragma once
#include <chrono>
#include <cstdint>

// NEW HOST (R30) load-time diagnostics (not recovered behaviour). Inactive unless the environment names the
// profiler report in SGN_PROFILE (host_profiler.hpp); the timeline is then written next to it, to
// "<SGN_PROFILE>.load.txt". The per-frame report keeps 2-second windows and is rewritten; this one keeps the
// whole process: for every stage its calls, total, average, worst and last duration and when it first
// started, plus milestones (e.g. the first presented frame) in milliseconds since the process was created.
// Any thread may record. The file is rewritten when a top-level stage (Stage) ends and at a milestone;
// nested per-item scopes (Scope) only accumulate.
namespace sgn::host_load_timeline
{
bool enabled() noexcept;
// `stage` must be a string with static storage duration (a literal).
void record(const char *stage, std::chrono::steady_clock::time_point start,
            std::chrono::steady_clock::time_point end) noexcept;
void milestone(const char *name) noexcept; // only its first occurrence is kept
void flush() noexcept;
double process_milliseconds() noexcept; // since process creation (Windows), else since first use
class Scope
{
    const char *stage_;
    bool flush_;
    bool active_;
    std::chrono::steady_clock::time_point start_{};

  public:
    Scope(const char *stage, bool flush_at_end) noexcept : stage_(stage), flush_(flush_at_end), active_(enabled())
    {
        if (active_)
            start_ = std::chrono::steady_clock::now();
    }
    ~Scope()
    {
        if (!active_)
            return;
        record(stage_, start_, std::chrono::steady_clock::now());
        if (flush_)
            flush();
    }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
};
} // namespace sgn::host_load_timeline
#define SGN_LOAD_TIMELINE_CONCAT2(a, b) a##b
#define SGN_LOAD_TIMELINE_CONCAT(a, b) SGN_LOAD_TIMELINE_CONCAT2(a, b)
// Per-item scope (accumulates only).
#define SGN_LOAD_SCOPE(name)                                                                                           \
    ::sgn::host_load_timeline::Scope SGN_LOAD_TIMELINE_CONCAT(sgn_load_scope_, __LINE__)(name, false)
// Top-level stage (rewrites the timeline file when it ends).
#define SGN_LOAD_STAGE(name)                                                                                           \
    ::sgn::host_load_timeline::Scope SGN_LOAD_TIMELINE_CONCAT(sgn_load_stage_, __LINE__)(name, true)
