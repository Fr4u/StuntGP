#pragma once
#include "sgn/retail_frontend_clock.hpp"
#include <cstdint>

namespace sgn::retail
{
// NEW HOST counter boundary (PORT-FIX proposal, R18 "P1"), not recovered code.
//
// 434820 returns only QueryPerformanceCounter's LowPart (43482A CALL [46E05C],
// 434830 MOV EAX,[EBP-8]); 434840 keeps QueryPerformanceFrequency's LowPart and
// derives the increment as LowPart/60 (43484A..434863). 442490 and the two
// catch-up loops (442834/442888, 441F1F/441F55) compare those 32-bit words
// unsigned. With the raw 10 MHz QPC the low word wraps every 2^32/1e7 s
// (7 min 9.5 s); when `observed` wraps before `scheduled` the loop test fails
// and 442490 (4424BD..4424D4) sees a small |delta|, so no tick runs until the
// 30-increment resync fires 0.5 s later. 441EC0 (441ED8..441EF0) only repairs
// the opposite order.
//
// Repair at the host boundary only: this counter reports QPC elapsed since
// construction, scaled to a virtual frequency. The source reads it through the
// unchanged 434820/434840/442490 code and arithmetic (32-bit truncation,
// increment=frequency/60, 30*increment resync, unsigned compares). 6000 Hz
// gives an exact increment of 100 (60 Hz ticks, 16.667 ms), a resolution of
// 166.7 us, and a first low-word wrap after 2^32/6000 s = 8 days 6 h 50 min
// of continuous process time.
class SourceHostRebasedCounter final : public FrontendPerformanceCounter
{
  public:
    static constexpr std::uint64_t virtual_frequency = 6000u;
    static_assert(virtual_frequency % 60u == 0u, "the source increment must stay exact");
    SourceHostRebasedCounter();         // captures the QPC base and native frequency
    std::uint64_t counter() override;   // floor((QPC-base)*6000/QPF), monotonic
    std::uint64_t frequency() override; // 6000
    // NEW HOST (interp R19): the same elapsed time in the same 6000 Hz units,
    // unfloored. Read only by the host presenter for its interpolation factor;
    // the source never reads it, so 434820/442490 results are unchanged.
    double precise_counter() const;

  private:
    std::int64_t base_{};
    std::int64_t native_frequency_{};
};
} // namespace sgn::retail
