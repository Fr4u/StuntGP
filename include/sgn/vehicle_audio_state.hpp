#pragma once
#include <array>
#include <cstdint>
namespace sgn::retail
{
// Single per-car owner. 413670 copies the complete value; 444DB0 clears it.
// Sound selectors 7708..7714 stay in VehicleLoadState.sounds. Contact fields
// 2360..236C/2378 and physics events2394/2398/23A0 stay in their existing owners.
struct VehicleAudioState
{
    std::uint32_t horn_handle{};                                                    // 2338, not cleared by41BDD0
    std::array<std::uint32_t, 4> fields_233c{};                                     // 233C..2348, role unverified
    std::uint32_t effect_hold{}, field_2350{}, effect_pending{}, field_2358{};      // 234C..2358
    float effect_strength{};                                                        // 235C
    std::uint32_t impact_handle{}, previous_wheel_touched{};                        // 2370,2374
    std::uint32_t grip_handle{}, scrape_handle{}, rolling_handle{}, boost_handle{}; // 237C..2388
    std::uint32_t idle_handle{}, rev_handle{}, motion_handle{};                     // 238C,2390,239C
    std::uint32_t field_7718{}, field_771c{}, field_7720{};                         // opaque shared load/clone words
};
// Single world owner at the replay frontier, never replay padding or per-car state.
struct VehicleAudioFrontier
{
    float previous_steering{};                                           // 570AC8
    std::uint32_t rotating_impact{}, steering_handle{}, steering_hold{}; // 570ACC..570AD4
};
} // namespace sgn::retail
