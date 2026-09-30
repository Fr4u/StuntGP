#pragma once
#include "sgn/retail_tick_clock.hpp"

namespace sgn::retail
{
// Mandatory source QueryPerformanceCounter/Frequency boundary. The host retains
// its64-bit time source; these legacy functions consume the returned low word.
// This interface does not select a display cadence or replace the modern driver.
struct FrontendPerformanceCounter
{
    virtual ~FrontendPerformanceCounter() = default;
    virtual std::uint64_t counter() = 0;
    virtual std::uint64_t frequency() = 0;
};
std::uint32_t read_frontend_counter_434820(FrontendPerformanceCounter &);
// 442100: two distinct reads with the scheduled store between them.
// Retains increment and frequency; does not initialize or select a clock.
void reset_frontend_clock_442100(RetailTickClock &, FrontendPerformanceCounter &);
void initialize_frontend_clock_434840(RetailTickClock &, std::uint32_t &source_frequency, FrontendPerformanceCounter &);
void sample_frontend_clock_442490(RetailTickClock &, std::uint32_t fixed_mode_a, std::uint32_t fixed_mode_b,
                                  FrontendPerformanceCounter &);
} // namespace sgn::retail
