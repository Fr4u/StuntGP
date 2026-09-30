#pragma once
#include <cstdint>

namespace sgn::retail
{
// Sole source scheduler words. Native wall-clock acquisition is a separate
// platform boundary; these values must never be scaled by display frame time.
struct RetailTickClock
{
    std::uint32_t scheduled{}, observed{}, increment{}; // 612D00/04/08
};
void advance_retail_tick(std::uint32_t &simulation_tick, RetailTickClock &) noexcept; // 441EC0
} // namespace sgn::retail
