#pragma once
#include <cstdint>
#include <memory>
#include <thread>

namespace sgn::retail
{
class RaceSessionPlatform;
class SourceMusicPeriodOwner;

// Explicit receiving seam. The same owned backend survives through teardown.
// Providers must not throw across the native-call boundary.
class SourceMusicPeriodCalls
{
  public:
    virtual ~SourceMusicPeriodCalls() = default;
    virtual std::uint32_t begin(std::uint32_t period) noexcept = 0;
    virtual std::uint32_t end(std::uint32_t period) noexcept = 0;
};

struct SourceMusicPeriodSnapshot
{
    std::uint64_t begin_calls{}, begin_successes{}, begin_failures{};
    std::uint64_t end_calls{}, end_successes{}, end_failures{};
    std::uint64_t pending_calls{}, unmatched_ends{}, teardown_end_calls{};
    std::uint64_t unresolved_at_teardown{};
    bool native_call_in_progress{}, finalized{}, counters_saturated{};
};

// Observation only; no source admission API accepts this report. Snapshot
// access checks the immutable creator thread before reading mutable counters.
// A copied snapshot can be transferred elsewhere; the report can outlive the
// platform without retaining the platform or any native request owner.
class SourceMusicPeriodReport final
{
    const std::thread::id thread_{std::this_thread::get_id()};
    SourceMusicPeriodSnapshot values_;
    friend class SourceMusicPeriodOwner;

  public:
    SourceMusicPeriodReport() = default;
    SourceMusicPeriodReport(const SourceMusicPeriodReport &) = delete;
    SourceMusicPeriodReport &operator=(const SourceMusicPeriodReport &) = delete;
    SourceMusicPeriodSnapshot snapshot() const;
};

// NEW host lifetime APIs only. Preparation performs no Begin/End and creates
// no window or audio device. Bind once to a cold source platform; replacement
// and coexistence with the legacy streaming backend are rejected.
std::shared_ptr<const SourceMusicPeriodReport> prepare_installed_music_period_owner(
    std::shared_ptr<RaceSessionPlatform>);
std::shared_ptr<const SourceMusicPeriodReport> prepare_receiving_music_period_owner(
    std::shared_ptr<RaceSessionPlatform>, std::shared_ptr<SourceMusicPeriodCalls>);

// Strong owner parameter spans the whole call and unwind. These APIs manage
// host obligations, not the complete retail45FDA0 or45FCE0 entry. The future
// source prefix must pass its already selected actual timer_period value.
// End requires one matching recorded Begin; an unmatched argument is rejected
// before a native call, as a NEW host precondition, never a retail result.
std::uint32_t begin_owned_music_period(std::shared_ptr<RaceSessionPlatform>, std::uint32_t period);
std::uint32_t end_owned_music_period(std::shared_ptr<RaceSessionPlatform>, std::uint32_t period);
} // namespace sgn::retail
