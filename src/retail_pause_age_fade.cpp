#include "sgn/retail_pause_age_fade.hpp"
#include "sgn/host_wide_race_anchors.hpp"

namespace sgn::retail
{
void draw_pause_age_fade_442200(PauseAgeFadeWorld world, PolygonSubmissionSink &sink)
{
    // Pinned D3D EXE4a1d234c...;442200..44222B use unsigned CMP/JC/SHR/JBE.
    if (world.frame_pause != 1)
        return;
    const auto age = world.age;
    if (age < 18000u)
        return;
    auto alpha = (age - 18000u) >> 1;
    if (alpha > 192u)
        alpha = 192u;
    auto d = world.descriptor;
    // 442230..4422AD: partial source stores in order. All other words persist.
    d[0] = 2;
    d[1] = 0;
    d[2] = 0;
    d[3] = 0x3f800000u;
    d[4] = 0x44200000u;
    d[5] = 0x43f00000u;
    d[11] = 0x48;
    d[12] = alpha;
    d[6] = 0x43000000u;
    d[7] = 0x43000000u;
    d[8] = 0x43000000u;
    d[9] = 0x43000000u;
    d[10] = 10;
    // 4422B2 tail: full existing clip/rectangle/blend/material/draw behavior.
    // NEW HOST (wide_hud R20): the flat 640x480 fade covers the widened view.
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::fade);
    submit_quad_432e00({d, world.vertices, world.selected_material, world.materials, world.viewport}, sink);
}
} // namespace sgn::retail
