#pragma once
#include "sgn/retail_race_hud.hpp"

namespace sgn::retail
{
// Full447BA0..447BC7. Current identifies a valid controller0..3; selected HUD
// is overwritten before access. No vehicle identity or active predicate is read.
void hide_selected_pause_hud_447ba0(RaceHudView, std::uint32_t ticks) noexcept;
// Full447C30..447C84, including real447BA0. Each of four controller vehicle
// identities must name a valid physical slot0..5, including inactive controllers.
// The saved current identity is restored. Slot4 is published transiently after
// the fourth visit, but never dereferenced. These synchronous borrowed views
// require distinct owners and no concurrent mutation; volatile is not a lock.
void hide_pause_hud_447c30(RaceHudView, std::span<const VehicleState, 6>, std::uint32_t ticks) noexcept;
} // namespace sgn::retail
