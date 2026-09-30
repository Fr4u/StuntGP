#pragma once
#include "sgn/race_session_world_tick.hpp"

namespace sgn::retail
{
// One entered iteration of 442810, instructions 442838..44287C. Input and
// camera outer preparation belong before the original catch-up loop and are
// not performed here. The caller owns cadence; no dt or display refresh input.
// The current supported demo/HUD domain is hud_block == 0 (40DE30 is a no-op).
// The exact 4423D0 gate may skip seven world calls; all five suffix calls remain.
// This function neither polls input nor changes vehicle input/history or an
// independent host recovery-key edge before the gate.
void advance_session_iteration(RaceSession &, RaceTextureReplacement &,
                               std::span<const contact::effects::EffectAnimationFrames>,
                               SessionWorldTickAudit * = nullptr);
} // namespace sgn::retail
