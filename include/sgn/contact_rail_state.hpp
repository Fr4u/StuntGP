#pragma once
#include "sgn/track_contact.hpp"
#include "sgn/source_camera_spline.hpp"

namespace sgn::retail
{
struct VehicleRailState
{
    contact::Vector4 effect_position{}; // 327C; XYZ writer00413E60, W retained
    std::uint32_t progress{};           // 7650, unsigned projection result, signed branch tests
    Vec3 projected_position{};          // 7664/7668/766C only; no ownership of7670
};
struct RailState
{
    CameraSplineBinding spline;                      // 004D5C9C; borrows active level PMD owner
    std::optional<std::uint32_t> tube_glass_texture; // 00474670, source -1 means absent
    std::uint32_t source_word_4d5ca0{};              // PE zero-fill; 004192E1 writes0; no inferred consumer
    // 004D5CA4/A8/AC already belong to VehicleResetGlobals and are borrowed
    // directly by the rail step. They must not be replicated here.
};
} // namespace sgn::retail
