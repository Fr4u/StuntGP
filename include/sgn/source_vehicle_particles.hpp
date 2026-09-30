#pragma once
#include "sgn/race_session_platform.hpp"

namespace sgn::retail
{
// R17 source-owner entries for the particle children of 418830 (418922..41893B)
// and of the replay path 418970 (41899D..4189B6). Each operates on the current
// vehicle (4D5BB8 == state().current_slot) and on the single world owners:
// state().effect_particles (62A078/62D478), state().effect_staging (4D5BC0/4D5C94),
// state().impact_particles (612D98..), state().random (4959C0), state().gravity
// (482668..), state().model_draws.template_pose (4864E8) and the actual source PMD
// model of vehicles.models[slot]. No snapshot, preview world or second RNG.
void emit_source_engine_particles_416a80(RaceSessionPlatform &);       // 416A80 + 4169E0
void emit_source_tire_particles_415880(RaceSessionPlatform &);         // 415880 + 4157F0
void emit_source_load_particles_417ca0(RaceSessionPlatform &);         // 417CA0 + 417B50
void emit_source_landing_particles_416f30(RaceSessionPlatform &);      // 416F30 + 416EA0
void emit_source_surface_particles_417800(RaceSessionPlatform &);      // 417800 + 4174E0/4175E0/4176F0
void emit_source_launch_wheel_particles_4173e0(RaceSessionPlatform &); // 4173E0 + 417350
} // namespace sgn::retail
