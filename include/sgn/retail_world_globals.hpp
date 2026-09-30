#pragma once
#include "sgn/vehicle_math.hpp"

namespace sgn::retail
{
// Single world owners. These three matrices are not per-controller matrices.
struct WorldFrameGlobals
{
    Vec3 field_497f20{}, field_497f70{};
    float field_497f80{}, field_497f84{};
    std::array<VehicleMatrix, 3> matrices{}; // 472210/472250/472290
};
struct WorldGravity
{
    float factor{}, inverse_factor{}, magnitude{}, inverse_magnitude{}; // 482668..674
    Vec3 vector{};                                                      // 482678..680
    VehicleVector unit{};                                               // 482688..694; W preserved by441F90
};
void initialize_world_frame_globals(WorldFrameGlobals &) noexcept; // full401DE0
void initialize_world_gravity(WorldGravity &) noexcept;            // full441F90, preserves factor/unit.W
} // namespace sgn::retail
