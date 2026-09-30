#include "sgn/retail_frame_border.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include "sgn/host_wide_overlay.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
std::uint32_t bits(double value)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}
void specular(FrameBorderWorld world, bool enable, PolygonSubmissionSink &sink)
{
    // Full reachable42F380/42F340 semantics.592050 is a source helper word,
    // not the device's render-state29 shadow, and is written before the call.
    if (world.legacy_driver_restriction == 1)
        return;
    world.submission.specular_enabled = enable ? 1u : 0u;
    sink.set_render_state(29, enable ? 1u : 0u);
    sink.set_render_state(24, enable ? 1u : 160u);
}
} // namespace
void draw_frame_border_431830(FrameBorderWorld world, std::uint32_t color, PolygonSubmissionSink &sink)
{
    if (world.submission.alpha_vertices.size() < 8)
        throw FormatError("frame border requires eight retained source alpha vertices");
    specular(world, false, sink); // 431839, before viewport reads
    const auto &vp = world.viewport;
    // NEW HOST (wide_hud R20): widened view rectangle on a wide canvas; in the
    // original presentation exactly double(width/height) and center_x/y.
    const auto view = host_wide_overlay::view_frame(vp);
    // Preserve x87 PC53 intermediates AND the individual binary32 local
    // stores. Using just cx +/- width/2 loses source rounding for large ints.
    const float width = static_cast<float>(view.width), height = static_cast<float>(view.height);
    const double half_width = view.width * 0.5;
    const float half_height = static_cast<float>(view.height * 0.5);
    const double left = double(view.center_x) - half_width;
    const float stored_left = static_cast<float>(left);
    const double top = double(view.center_y) - double(half_height);
    const float stored_top = static_cast<float>(top + 0.5);
    const double right = (double(stored_left) + double(width)) - 0.5;
    const float stored_bottom = static_cast<float>((top + double(height)) - 0.5);
    const auto lx = bits(left), rx = bits(right), ty = std::bit_cast<std::uint32_t>(stored_top),
               by = std::bit_cast<std::uint32_t>(stored_bottom);
    const std::array<std::uint32_t, 8> x{lx, bits(right + 0.5), rx, rx, rx, lx, lx, lx};
    const std::array<std::uint32_t, 8> y{ty, ty, ty, bits(double(stored_bottom) + 0.5),
                                         by, by, by, bits(double(stored_top) - 0.5)};
    auto scratch = std::span(world.submission.alpha_vertices).first<8>();
    for (std::size_t i = 0; i < 8; ++i)
    {
        auto &v = scratch[i].words;
        v[0] = x[i];
        v[1] = y[i];
        v[2] = v[3] = 0x3f70a3d7u;
        v[4] = color | 0xff000000u;
        v[5] = 0xff000000u;
    }
    apply_screen_polygon_blend_432190(0x28, sink); // 431A20, actual existing helper
    sink.set_texture(0, 0);
    sink.draw(2, 0x2c4, scratch, 0x14, vp);
    const auto quality = world.quality;    // 431A52, before the source cache store
    world.selected_material = 0xffffffffu; // 431A57, even if drawing failed
    if (quality == 1)
        specular(world, true, sink);
}
} // namespace sgn::retail
