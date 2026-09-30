#pragma once
#include "sgn/retail_polygon_submission.hpp"
#include <cstdint>
#include <span>
#include <vector>

// NEW HOST (R29) scroll arrows for the GRAPHICS menus (in-race ADVANCED page
// and the Options GRAPHICS page). Not recovered behaviour and not a retail
// asset: the 2001 game has no arrow art. The arrow is procedural vector art in
// the style of the pause box's "PAUSE" lettering (BoxBack, overlay.wad): a
// chunky, slightly italic triangle with the lettering's vertical gradient (red
// top -> orange -> yellow bottom, sampled from the BoxBack texels), a dark navy
// outline and a soft drop shadow. It is drawn as untextured, vertex-coloured
// triangles (texture token 0) with a one-output-pixel alpha feather on every
// outer edge, so it is anti-aliased at any resolution and MSAA setting and is
// independent of sharp_hud, mipmaps and the texture edge fix (it samples no
// texture). The polygon texture pipeline only carries the game's A1R5G5B5
// (1-bit alpha) material tokens, which is why no RGBA texture is registered.
namespace sgn::host_menu_arrow
{
enum class Direction : std::uint8_t
{
    up,
    down
};
struct Vertex
{
    float x{}, y{};       // 640x480 virtual units (before the overlay mapping)
    std::uint32_t argb{}; // straight (non-premultiplied) alpha, blended SRCALPHA/INVSRCALPHA
};
struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices; // triangle list
};
// Width of the arrow in virtual units; height is 0.62 * width.
inline constexpr float default_width = 16.0f;
// `feather` is the anti-aliasing band in virtual units (one output pixel:
// 1 / host_wide::current().pixels_per_unit).
Mesh build(Direction, float center_x, float center_y, float width, float feather);
// Feather of one output pixel for the current host layout.
float one_pixel_feather() noexcept;

// What the sink must look like after the arrows (the draw leaves the polygon
// device as the caller's source code expects it).
struct Restore
{
    std::uint32_t texture_token{}; // token of the material the source believes is bound (61C5E4), 0 if none
    std::uint32_t alpha_ref{};     // ALPHAREF (render state 24) after the preceding source draw
};
// Draws the meshes in order through `sink` (one indexed triangle list each):
// the states of 432190 alpha mode 0x48 (blend SRCALPHA/INVSRCALPHA), ALPHAREF 1, texture
// token 0; then ALPHABLENDENABLE 0, `restore.alpha_ref` and the restored token.
// Vertex positions are mapped like 4291F0 glyphs through
// host_wide_overlay::overlay_mapping(viewport); z = rhw = depth.
void draw(retail::PolygonSubmissionSink &, const retail::ViewportState &, std::span<const Mesh>, float depth,
          const Restore &restore);
} // namespace sgn::host_menu_arrow
