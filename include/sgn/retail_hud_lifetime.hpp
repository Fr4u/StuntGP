#pragma once
#include "sgn/retail_race_hud.hpp"

namespace sgn::retail
{
void update_overlay_transition(ControllerOverlayTransition &); // complete442B60
void update_controller_overlays(contact::player::Controls &, std::span<ControllerOverlayTransition, 4>); // 442BE0
void update_hud_element(HudElementView);                           // complete447AB0
void age_controller_notices(contact::player::Controls &) noexcept; // complete442DE0
void update_race_hud(RaceHudView, std::span<const VehicleState, 6>,
                     std::span<const VehicleRaceState, 6>); // 447C90 +442DE0
} // namespace sgn::retail
