#pragma once
#include "sgn/host_wide_layout.hpp"
#include <array>
#include <cstdint>

namespace sgn::retail
{
struct ViewportState;
}

// NEW HOST presentation (wide_hud, R20). Not recovered game behaviour.
//
// Shared model A (host_wide_layout.hpp, owned by wide_layout): every source
// owner keeps its 640x480 frame; the renderer shows the whole canvas, which
// extends the source frame by extend_x/extend_y per side. Untouched 2D is
// therefore centred 4:3. The original maps each 2D overlay from the 640x480
// logical space with (x-320)*ui_scale_x+center_x, (y-240)*ui_scale_y+center_y
// (ui_scale 624990/624994 from 425070, center 576B94/576B98, clip 576C04).
//
// An AnchorScope changes, for the overlay elements submitted inside it, only
// the host-side mapping used by the drawing chokepoints (432E00 quads,
// 4291F0/4323F0 text, 447840 UI lines, 4428A0/445800 loading quads):
// * at(h,v): the scale stays ui_scale (glyph aspect unchanged); center moves
//   by -extend_left/+extend_right (-extend_top/+extend_bottom) of the current
//   view (host_wide::widened_view), so the element keeps its distance to that
//   view edge; the logical clip is widened to the widened view edges so
//   slide-in animations enter at the real screen edge.
// * fill(): the 640x480 logical frame is stretched onto the widened view.
//   Only for flat edge-to-edge fills (fades) whose single texel makes the
//   stretch invisible.
// Outside any scope (Anchor::original()) and whenever widened_view(v) is not
// active (--aspect-4x3 / SGN_ASPECT=4x3, degenerate viewport) the mapping is
// the viewport's own floats, bit for bit. Nothing here writes a source owner.
// Owner (draw) thread only for the scope; the layout itself is wide_layout's.
namespace sgn::host_wide_overlay
{

using Horizontal = host_wide::Horizontal;
using Vertical = host_wide::Vertical;

struct Anchor
{
    enum class Mode : std::uint8_t
    {
        original,
        edge,
        fill
    };
    Horizontal horizontal{Horizontal::center};
    Vertical vertical{Vertical::middle};
    Mode mode{Mode::original};
    static constexpr Anchor original() noexcept
    {
        return {};
    }
    static constexpr Anchor at(Horizontal h, Vertical v) noexcept
    {
        return {h, v, Mode::edge};
    }
    static constexpr Anchor fill() noexcept
    {
        return {Horizontal::center, Vertical::middle, Mode::fill};
    }
    constexpr bool operator==(const Anchor &) const = default;
};

// Host mapping of one draw call. For the original anchor or an inactive
// widened view: scale/center are the viewport's own floats and clip is its
// clip_virtual (x0,x1,y0,y1), bit for bit, and anchored is false.
struct OverlayMapping
{
    float scale_x{}, scale_y{}, center_x{}, center_y{};
    std::array<float, 4> clip{}; // logical x0,x1,y0,y1
    bool anchored{};
};

// Current view frame in source raster units (431830 border). Identity:
// double(width/height) and the viewport's center floats.
struct ViewFrame
{
    double width{}, height{};
    float center_x{}, center_y{};
};

// Host kill switch (default on). Off = original mapping everywhere.
void set_enabled(bool) noexcept;
bool enabled() noexcept;

Anchor current_anchor() noexcept;
OverlayMapping overlay_mapping(const retail::ViewportState &) noexcept;
// NEW HOST (widescreen R37): visible region beyond the current view's original
// edge on one side, in logical (640x480) units (widened view growth divided by
// ui_scale). 0 for Horizontal::center, on a 4:3 canvas / original_4x3, when the
// host overlay is disabled or the viewport is degenerate. Used to lengthen
// slide paths designed to end just beyond the 4:3 edge.
double side_extension(const retail::ViewportState &, Horizontal) noexcept;
ViewFrame view_frame(const retail::ViewportState &) noexcept;

class AnchorScope
{
    Anchor previous_;

  public:
    explicit AnchorScope(Anchor) noexcept;
    ~AnchorScope();
    AnchorScope(const AnchorScope &) = delete;
    AnchorScope &operator=(const AnchorScope &) = delete;
};

} // namespace sgn::host_wide_overlay
