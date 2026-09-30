#pragma once
#include "sgn/vehicle_score.hpp"
#include "sgn/retail_audio_driver.hpp"

namespace sgn::retail
{
// Sole popup bank, delayed sound cache and RNG remain in the existing Session.
void update_score_audio_delay(VehicleScoreState &, AudioDriver &);                           // full4011F0
void spawn_score_sparkle(VehicleScoreState &, VehicleRandom &, float x, float y);            // full401030
void update_score_popups(VehicleScoreState &, VehicleRandom &, Vec3 gravity, AudioDriver &); // full401770
} // namespace sgn::retail
