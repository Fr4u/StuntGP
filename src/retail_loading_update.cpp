#include "sgn/retail_loading_update.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sgn::retail
{
void update_loading_presentation_4458f0(LoadingPresentationUpdateWorld world, FrontendPerformanceCounter &source)
{
    if (world.clock.increment == 0 || world.clock.increment > std::numeric_limits<std::uint32_t>::max() / 60u)
        throw std::invalid_argument("loading source clock has no valid initialized increment");
    sample_frontend_clock_442490(world.clock, world.fixed_mode_a, world.fixed_mode_b, source);
    while (world.clock.scheduled < world.clock.observed)
    {
        update_controller_overlays(world.controls, world.overlays);
        advance_retail_tick(world.simulation_tick, world.clock);
        std::copy(world.keys.begin(), world.keys.end(), world.previous_keys.begin());
    }
}
} // namespace sgn::retail
