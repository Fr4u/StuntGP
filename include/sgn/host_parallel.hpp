#pragma once
#include <chrono>
#include <cstdint>
#include <functional>

// NEW HOST (R30) CPU parallelism for host load-time work (texture conversion, asset reads). Not recovered
// behaviour: nothing here runs source code or touches a source owner.
//
// Band runs: for_bands(count, grain, limit, body) splits [0, count) into contiguous bands and runs
// body(first, last) once per band on a persistent worker pool (created on first use, never torn down; the
// calling thread takes part and waits). A body must compute only its own items of pre-sized outputs from
// shared read-only inputs, so the result is identical for any band count, thread count or schedule. The
// band count is min(limit, pool threads + 1, count / grain), at least 1. With one band, on a pool or
// background thread, inside a band (a nested run), or while another thread is running bands, the whole
// range runs inline on the caller as body(0, count). An exception of a band is rethrown on the caller
// after every band has finished (the one of the lowest band when several throw).
namespace sgn::host_parallel
{
using BandBody = void (*)(const void *context, std::uint32_t first, std::uint32_t last);
void run_bands(std::uint32_t count, std::uint32_t grain, unsigned limit, const void *context, BandBody body);
template <class Body> void for_bands(std::uint32_t count, std::uint32_t grain, unsigned limit, const Body &body)
{
    run_bands(count, grain, limit, &body, [](const void *context, std::uint32_t first, std::uint32_t last) {
        (*static_cast<const Body *>(context))(first, last);
    });
}
// Threads a band run may use (pool workers + the caller), at least 1.
unsigned concurrency() noexcept;

// Background-mode threads (R35: up to 2, a sixth of the logical CPUs; lowest CPU, I/O and memory
// priority), tasks started in FIFO order (they may finish in any order). A task's exception is swallowed.
// Background tasks never use the pool (their band runs are inline), so they cannot slow a foreground band
// run. Returns false (task dropped) only when the queue could not take it.
bool post_background(std::function<void()> task) noexcept;
bool on_background_thread() noexcept;
bool on_pool_thread() noexcept;

// Profiler bridge for code that cannot link the D3D12 host profiler (host_profiler.hpp). The platform side
// installs host_profile::add. The sink is only used on a thread that is neither a pool nor a background
// thread (host_profile is a game-thread-only accumulator).
using ProfileSink = void (*)(const char *stage, std::int64_t nanoseconds);
void set_profile_sink(ProfileSink) noexcept;
ProfileSink profile_sink() noexcept; // null when unset or on a pool/background thread
class ProfileScope
{
    ProfileSink sink_;
    const char *stage_;
    std::chrono::steady_clock::time_point start_{};

  public:
    explicit ProfileScope(const char *stage) noexcept : sink_(profile_sink()), stage_(stage)
    {
        if (sink_)
            start_ = std::chrono::steady_clock::now();
    }
    ~ProfileScope()
    {
        if (sink_)
            sink_(stage_,
                  std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start_)
                      .count());
    }
    ProfileScope(const ProfileScope &) = delete;
    ProfileScope &operator=(const ProfileScope &) = delete;
};
} // namespace sgn::host_parallel
#define SGN_HOST_PARALLEL_CONCAT2(a, b) a##b
#define SGN_HOST_PARALLEL_CONCAT(a, b) SGN_HOST_PARALLEL_CONCAT2(a, b)
#define SGN_HOST_PROFILE_SCOPE(name)                                                                                   \
    ::sgn::host_parallel::ProfileScope SGN_HOST_PARALLEL_CONCAT(sgn_host_profile_scope_, __LINE__)(name)
