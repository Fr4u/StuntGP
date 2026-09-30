#pragma once
#include "sgn/retail_replay_decode.hpp"
#include "sgn/contact_solver.hpp"

namespace sgn::retail
{
void decode_replay_steering(const ReplayFramePair &, VehicleState &);                                       // 0041A330
void decode_replay_suspension(const ReplayFramePair &, VehicleState &);                                     // 0041A3C0
void decode_replay_loads(const ReplayFramePair &, VehicleState &, contact::solver::State &, float gravity); // 0041A430
void decode_replay_turbo(const ReplayFramePair &, VehicleState &);                                          // 0041A8D0
void decode_replay_gear(const ReplayFramePair &, VehicleState &);      // 0041A7E0, 0040C160
void decode_replay_auxiliary(const ReplayFramePair &, VehicleState &); // 0041A9B0
void decode_replay_brake(const ReplayFramePair &, VehicleState &);     // 0041A900
void decode_replay_rpm(const ReplayFramePair &, VehicleState &);       // 0041A370
void decode_replay_throttle(const ReplayFramePair &, VehicleState &);  // 0041A930
// All four effect words +2360..236C have one owner in contact::solver::State.
void decode_replay_shock(const ReplayFramePair &, contact::solver::State &);   // 0041A9E0
void decode_replay_surface(const ReplayFramePair &, contact::solver::State &); // 0041AA60
} // namespace sgn::retail
