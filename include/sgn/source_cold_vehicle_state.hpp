#pragma once
namespace sgn::retail
{
class RaceSessionPlatform;
// Actual40A2F0 on the retained source world. No fresh Session or whole-world copy.
// Requires source DF=0 and no observer/native callback inside the source REP.
void initialize_source_cold_vehicle_state_40a2f0(RaceSessionPlatform &);
void initialize_source_world_frame_globals_401de0(RaceSessionPlatform &);
} // namespace sgn::retail
