#include "sgn/retail_frontend_clock.hpp"

namespace sgn::retail
{
std::uint32_t read_frontend_counter_434820(FrontendPerformanceCounter &source)
{
    return static_cast<std::uint32_t>(source.counter());
}
void reset_frontend_clock_442100(RetailTickClock &clock, FrontendPerformanceCounter &source)
{
    clock.scheduled = read_frontend_counter_434820(source); // 442100..442105
    clock.observed = read_frontend_counter_434820(source);  // 44210A..44210F
}
void initialize_frontend_clock_434840(RetailTickClock &clock, std::uint32_t &source_frequency,
                                      FrontendPerformanceCounter &source)
{
    const auto frequency = static_cast<std::uint32_t>(source.frequency());
    source_frequency = frequency;      // 43485D before434863
    clock.increment = frequency / 60u; // unsigned MUL88888889/SHR5
    clock.scheduled = read_frontend_counter_434820(source);
    clock.observed = read_frontend_counter_434820(source); // second distinct source read
}
void sample_frontend_clock_442490(RetailTickClock &clock, std::uint32_t fixed_mode_a, std::uint32_t fixed_mode_b,
                                  FrontendPerformanceCounter &source)
{
    if (fixed_mode_a == 1 || fixed_mode_b == 1)
    {
        clock.scheduled = clock.observed;
        clock.observed += clock.increment;
        return;
    }
    const auto observed = read_frontend_counter_434820(source);
    const auto delta = observed - clock.scheduled;
    clock.observed = observed;
    const auto sign = 0u - (delta >> 31);
    const auto magnitude = (delta ^ sign) - sign; // CDQ/XOR/SUB, includesINT_MIN
    const auto threshold = clock.increment * 30u; // original32-bit LEA/SHL wrap
    if (magnitude > threshold)
        clock.scheduled = observed - 1u;
}
} // namespace sgn::retail
