#pragma once
// NEW HOST (R33 perf-pass-parallel), not recovered behaviour. Runs a pure per-element kernel of the source
// race pass on the persistent host_parallel pool (host_parallel.hpp), synchronously: the call returns only
// after every element is done, so no pass_yield_point() and no other source work can run while a kernel is
// in flight.
//
// Contract for `body(first, last)` (checked by the callers' equivalence arguments, see STATUS.txt):
//  - it computes elements [first, last) with exactly the code that ran serially for them (same function,
//    same operation order), reading only data that no element writes and writing only its own elements'
//    disjoint output bytes; it is noexcept and touches no source owner, bank, allocator or profiler;
//  - so every element's output is a function of read-only inputs only, and the result is the same for any
//    band split, thread count or schedule.
// Floating-point environment: the kernels are SSE2 (x64 C++). A worker runs its band under the caller's
// MXCSR control bits (rounding, masks, FTZ/DAZ: 429510 mirrors the source x87 control word there), and the
// exception flags its band raised are ORed back into the caller's MXCSR afterwards, so the caller's MXCSR
// ends exactly as after the serial loop (sticky flags are a union over the same operations). Worker MXCSR is
// restored after each band.
// Small counts run inline on the caller (the pool's wake-up would cost more than it saves). Never throws: if
// the pool itself fails, the whole range runs inline.
#include "sgn/host_parallel.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <xmmintrin.h>

namespace sgn::retail::detail
{
inline constexpr std::uint32_t pass_parallel_minimum = 512; // elements; below this the loop stays serial
inline constexpr std::uint32_t pass_parallel_grain = 256;   // elements per band at least
inline constexpr unsigned pass_parallel_band_limit = 8;
// Returns true when the pool was asked to split the range (it may still have run inline).
template <class Body> bool pass_parallel_for(std::uint32_t count, const Body &body) noexcept
{
    static_assert(noexcept(body(std::uint32_t{}, std::uint32_t{})), "pass kernels must be noexcept");
    if (count < pass_parallel_minimum || host_parallel::concurrency() < 2)
    {
        body(0, count);
        return false;
    }
    constexpr unsigned flag_bits = 0x3fu; // MXCSR IE DE ZE OE UE PE
    const unsigned caller_csr = _mm_getcsr();
    const auto caller = std::this_thread::get_id();
    std::atomic<unsigned> raised{0};
    try
    {
        host_parallel::for_bands(count, pass_parallel_grain, pass_parallel_band_limit,
                                 [&](std::uint32_t first, std::uint32_t last) noexcept {
                                     if (std::this_thread::get_id() == caller)
                                     {
                                         body(first, last); // flags accumulate in the caller's MXCSR as before
                                         return;
                                     }
                                     const unsigned own = _mm_getcsr();
                                     _mm_setcsr(caller_csr & ~flag_bits);
                                     body(first, last);
                                     raised.fetch_or(_mm_getcsr() & flag_bits, std::memory_order_relaxed);
                                     _mm_setcsr(own);
                                 });
    }
    catch (...)
    {
        // The pool could not run (e.g. it failed to start). Elements are pure functions of read-only inputs,
        // so computing all of them again here stores the same bytes as any partial parallel run did.
        body(0, count);
    }
    if (const auto flags = raised.load(std::memory_order_relaxed))
        _mm_setcsr(_mm_getcsr() | flags);
    return true;
}
// Two byte ranges [a, a+na) and [b, b+nb) overlap.
inline bool pass_ranges_overlap(const void *a, std::size_t na, const void *b, std::size_t nb) noexcept
{
    const auto x = reinterpret_cast<std::uintptr_t>(a), y = reinterpret_cast<std::uintptr_t>(b);
    return x < y + nb && y < x + na;
}
} // namespace sgn::retail::detail
