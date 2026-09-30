#include "sgn/retail_tick_clock.hpp"

namespace sgn::retail
{
void advance_retail_tick(std::uint32_t &simulation_tick, RetailTickClock &clock) noexcept
{
    ++simulation_tick;
    clock.scheduled += clock.increment;
    if (clock.scheduled < 0x10000000u && clock.observed > 0x80000000u)
        clock.scheduled = clock.observed;
}
} // namespace sgn::retail
