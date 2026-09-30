#pragma once
#include "sgn/retail_frontend_clock.hpp"
#include "sgn/retail_hud_lifetime.hpp"

namespace sgn::retail
{
struct LoadingPresentationUpdateWorld
{
    RetailTickClock &clock;
    std::uint32_t &simulation_tick;    // 612CFC, sole world counter
    const std::uint32_t &fixed_mode_a; // 60F2AC
    const std::uint32_t &fixed_mode_b; // 60DF9C
    contact::player::Controls &controls;
    std::span<ControllerOverlayTransition, 4> overlays;
    std::span<const std::uint32_t, 64> keys;    // RaceInitializationState.fields_62d480
    std::span<std::uint32_t, 64> previous_keys; // same owner's fields_62dcf0
};
// Full4458F0. Input timing must be initialized by the source frequency domain:
// increment1..UINT32_MAX/60. Zero increment is rejected as a host safety guard.
// The loop updates overlays, publishes the source tick, then copies64 words.
void update_loading_presentation_4458f0(LoadingPresentationUpdateWorld, FrontendPerformanceCounter &);
} // namespace sgn::retail
