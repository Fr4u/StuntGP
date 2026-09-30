#pragma once
#include <cstdint>
#include <string_view>

namespace sgn::retail
{
struct ViewportState;
}

// NEW HOST presentation (R20 widescreen, owned by wide_layout = wide_3d).
// Not recovered game behaviour. Nothing here is read by the simulation.
//
// Shared model ("model A", see COORDINATION.md):
// * Every source owner keeps its original 640x480 frame. ViewportState
//   (576B80..576C30), ui_scale, the captured projection extent of each draw
//   and every value the tick reads are unchanged. Source coordinates of a
//   point that was visible in the original are unchanged.
// * The LOGICAL CANVAS is the 640x480 source frame extended symmetrically:
//   canvas height 480 and width 480*aspect when aspect >= 4:3 (extend_x per
//   side), or width 640 and height 640/aspect when aspect < 4:3 (extend_y per
//   side). In source (virtual 640x480) units the canvas spans
//   x in [-extend_x, 640+extend_x], y in [-extend_y, 480+extend_y].
//   Untouched 2D content is therefore centred 4:3 and never stretched; an
//   element anchored to an edge moves by -extend (left/top) or +extend
//   (right/bottom). Nothing outside [0,640]x[0,480] was visible before.
// * The renderer (PolygonD3D12::record) maps the whole canvas onto the whole
//   output target with one uniform scale (pixels_per_unit), at native output
//   resolution: no bars, no stretching.
// * The original presentation (--aspect-4x3 or SGN_ASPECT=4x3): canvas is
//   exactly 640x480, extend 0, and the renderer keeps today's centred
//   aspect-fit (black bars). Every consumer must be the identity then.
// * Race 3D is Hor+ (wide_3d): vertical FOV unchanged, clip/cull bounds widen
//   to the canvas; a split-screen view rectangle is scaled about the canvas
//   centre by (scale_x, scale_y), see widened_view().
//
// Units: "virtual" = the 640x480 space of 2D layout (set_viewport_clip,
// ui_scale). The source raster normally is 640x480 too; if the boot config
// selects another source raster (full_width != 640) the per-view helpers
// below convert with full_width/640 and full_height/480.
//
// Thread safety: the layout is one atomic snapshot; any thread may read it.
// set_output_size is called by the renderer on creation/resize.
namespace sgn::host_wide
{

inline constexpr float source_width = 640.0f, source_height = 480.0f;

enum class Horizontal : std::int8_t
{
    left = -1,
    center = 0,
    right = 1
};
enum class Vertical : std::int8_t
{
    top = -1,
    middle = 0,
    bottom = 1
};

struct Layout
{
    std::uint32_t output_width{640}, output_height{480};            // physical target pixels
    bool original_4x3{};                                            // --aspect-4x3 / SGN_ASPECT=4x3
    float canvas_width{source_width}, canvas_height{source_height}; // virtual units
    float extend_x{}, extend_y{};                                   // per side, virtual units
    float pixels_per_unit{1};                                       // output pixels per virtual unit (uniform)
    float output_origin_x{}, output_origin_y{}; // output pixel of canvas (-extend_x,-extend_y); bars in 4:3 mode

    bool wide() const noexcept
    {
        return extend_x > 0.0f || extend_y > 0.0f;
    }
    // Hor+/Vert+ factors: canvas extent / source extent (1 in 4:3 mode).
    float scale_x() const noexcept
    {
        return canvas_width / source_width;
    }
    float scale_y() const noexcept
    {
        return canvas_height / source_height;
    }
    float left() const noexcept
    {
        return -extend_x;
    }
    float right() const noexcept
    {
        return source_width + extend_x;
    }
    float top() const noexcept
    {
        return -extend_y;
    }
    float bottom() const noexcept
    {
        return source_height + extend_y;
    }
    // Moves a virtual 4:3 coordinate with its anchor: left/top -extend,
    // right/bottom +extend, center/middle unchanged. Identity in 4:3 mode.
    float anchor_x(Horizontal a, float x) const noexcept
    {
        return a == Horizontal::left ? x - extend_x : a == Horizontal::right ? x + extend_x : x;
    }
    float anchor_y(Vertical a, float y) const noexcept
    {
        return a == Vertical::top ? y - extend_y : a == Vertical::bottom ? y + extend_y : y;
    }
    // Virtual canvas <-> output pixel (mouse/cursor mapping, diagnostics).
    float output_x(float virtual_x) const noexcept
    {
        return output_origin_x + (virtual_x + extend_x) * pixels_per_unit;
    }
    float output_y(float virtual_y) const noexcept
    {
        return output_origin_y + (virtual_y + extend_y) * pixels_per_unit;
    }
    float virtual_x(float output_px) const noexcept
    {
        return (output_px - output_origin_x) / pixels_per_unit - extend_x;
    }
    float virtual_y(float output_py) const noexcept
    {
        return (output_py - output_origin_y) / pixels_per_unit - extend_y;
    }
};

// Pure function (no global state). width/height 0 are treated as 640x480.
// Canvas extent is clamped to [640,8192]x[480,8192] virtual units.
Layout compute_layout(std::uint32_t output_width, std::uint32_t output_height, bool original_4x3) noexcept;

// Current process-wide layout (atomic snapshot of output size + mode).
Layout current() noexcept;
void set_output_size(std::uint32_t width, std::uint32_t height) noexcept;
bool original_4x3() noexcept;
void set_original_4x3(bool) noexcept;
// Host startup: `--aspect-4x3` in the command line (case-insensitive token)
// or environment SGN_ASPECT=4x3 selects the original presentation. Returns
// the resulting original_4x3() value. Default: widescreen on.
bool configure(std::wstring_view command_line) noexcept;

// One source view rectangle (a selected ViewportState: selector 1..16) in
// SOURCE RASTER units, before and after widening. In 4:3 mode (or when the
// viewport is not the full 4:3 frame's subdivision) widened == original.
// Rule: the view is scaled about the canvas centre (origin + full/2) by the
// layout factors, so a view touching a 4:3 edge now touches the canvas edge
// and a centred view keeps its centre (selector 1: [-ext, 640+ext]).
struct ViewRect
{
    float center_x{}, center_y{}, half_width{}, half_height{};
    float left() const noexcept
    {
        return center_x - half_width;
    }
    float right() const noexcept
    {
        return center_x + half_width;
    }
    float top() const noexcept
    {
        return center_y - half_height;
    }
    float bottom() const noexcept
    {
        return center_y + half_height;
    }
};
struct WideView
{
    bool active{};                  // false: identity (4:3 mode or degenerate viewport)
    float factor_x{1}, factor_y{1}; // Hor+/Vert+ factor for this view (layout scale_x/scale_y)
    ViewRect original, widened;     // source raster units
    // Per-side growth in source raster units (widened edge - original edge, >= 0).
    float extend_left{}, extend_right{}, extend_top{}, extend_bottom{};
    // Widened 576C04..30 clip rectangles for 3D screen-space consumers
    // (x0,x1,y0,y1), computed like 4250A0 from the widened view.
    float clip_screen[4]{}, clip_expanded[4]{};
};
WideView widened_view(const Layout &, const retail::ViewportState &) noexcept;
inline WideView widened_view(const retail::ViewportState &v) noexcept
{
    return widened_view(current(), v);
}
// Same rule with explicit factors (e.g. factors recorded with a captured
// frame). factor_x==factor_y==1 returns the identity (active=false).
WideView widened_view(float factor_x, float factor_y, const retail::ViewportState &) noexcept;

} // namespace sgn::host_wide
