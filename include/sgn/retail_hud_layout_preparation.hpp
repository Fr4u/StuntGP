#pragma once
#include "sgn/retail_race_hud.hpp"

namespace sgn::retail
{
// Full447D80. Only selected_hud is consumed. Current controller/vehicle may
// differ. ComputedXY are the PREVIOUS element's existing word_1c/word_20;
// authored element.x/y remain untouched. No additional coordinate owner.
void prepare_hud_layout_447d80(RaceHudView);
// Full44AC90. Read current controller's actual vehicle displacement X/Y,
// clamp independently, then add to the selected HUD's same computed words.
// Neither owner selector nor the vehicle displacement is changed. No tick.
void apply_hud_displacement_44ac90(RaceHudView, std::span<const VehicleState, 6>);
} // namespace sgn::retail
