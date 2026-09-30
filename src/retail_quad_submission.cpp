#include "sgn/retail_quad_submission.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include "sgn/vehicle_math.hpp"
#include "sgn/host_wide_overlay.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using Input = std::span<const std::uint32_t, 35>;
using sgn::bit_reinterpret::as_float;
std::uint32_t word(double value)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}
bool below(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
using sgn::bit_reinterpret::as_signed;
using sgn::format_check::require;
VehicleMatrix rotation(Input d)
{
    const auto angle = as_float(d[13]);
    const double index = double(angle) * double(std::bit_cast<float>(0x41b60b61u));
    require(std::isfinite(index) && index >= -2147483648.0 && index <= 2147483647.0,
            "quad rotation outside recovered angle domain");
    return vehicle_rotation(2, angle);
}
void rotate(float &x, float &y, Input d, const VehicleMatrix &matrix)
{
    const double dx = double(x) - as_float(d[14]), dy = double(y) - as_float(d[15]);
    // 432B6B/432CD7 store X before adding the pivot; Y has no such store.
    const float intermediate = static_cast<float>((dy * matrix[4] + dx * matrix[0]) + matrix[12]);
    const double next_y = (dx * matrix[1] + dy * matrix[5]) + matrix[13];
    x = static_cast<float>(double(intermediate) + as_float(d[14]));
    y = static_cast<float>(next_y + as_float(d[15]));
}
void scale(float &x, float &y, Input d)
{
    x = static_cast<float>((double(x) - as_float(d[14])) * as_float(d[16]) + as_float(d[14]));
    y = static_cast<float>((double(y) - as_float(d[15])) * as_float(d[17]) + as_float(d[15]));
}
} // namespace
bool clip_quad_4251a0(std::span<std::uint32_t, 35> d, const ViewportState &viewport)
{
    double width, height;
    if (d[0] & 8)
    {
        const auto px = as_float(d[14]), py = as_float(d[15]);
        d[1] = word((double(as_float(d[1])) - double(px) * as_float(d[16])) + px);
        d[14] = 0;
        d[2] = word((double(as_float(d[2])) - double(py) * as_float(d[17])) + py);
        d[15] = 0;
        width = static_cast<float>(double(as_float(d[4])) * as_float(d[16])); // 4251D9 FSTP
        height = double(as_float(d[17])) * as_float(d[5]);                    // height retained on x87 stack
    }
    else
    {
        width = as_float(d[4]);
        height = as_float(d[5]);
    }
    float x = as_float(d[1]), y = as_float(d[2]);
    const float right = static_cast<float>(double(x) + width), bottom = static_cast<float>(double(y) + height);
    const auto &c = viewport.clip_virtual;
    // These four source decisions compare signed IEEE bits after a float store.
    if (as_signed(word(double(right) - c[0])) <= 0 || as_signed(word(double(x) - c[1])) >= 0 ||
        as_signed(word(double(bottom) - c[2])) <= 0 || as_signed(word(double(y) - c[3])) >= 0)
        return false;
    if (!below(x, c[0]) && below(right, c[1]) && !below(y, c[2]) && below(bottom, c[3]))
        return true;
    double u0 = as_float(d[6]), v0 = as_float(d[7]), u1 = as_float(d[8]), v1 = as_float(d[9]);
    if (below(x, c[0]))
    {
        const float delta = static_cast<float>(double(c[0]) - x);
        u0 += (u1 - u0) * double(delta) / width;
        width -= delta;
        x = static_cast<float>(double(delta) + x);
    }
    if (double(right) > c[1])
    {
        const double delta = double(right) - c[1];
        u1 -= (u1 - u0) * delta / width;
        width -= delta;
    }
    if (below(y, c[2]))
    {
        const double delta = double(c[2]) - y;
        v0 += (v1 - v0) * delta / height;
        height -= delta;
        y = static_cast<float>(delta + y);
    }
    if (double(bottom) > c[3])
    {
        const double delta = double(bottom) - c[3];
        v1 -= (v1 - v0) * delta / height;
        height -= delta;
    }
    d[4] = word(width);
    d[5] = word(height);
    d[6] = word(u0);
    d[8] = word(u1);
    d[7] = word(v0);
    d[2] = std::bit_cast<std::uint32_t>(y);
    d[9] = word(v1);
    d[17] = d[16] = 0x3f800000;
    d[1] = std::bit_cast<std::uint32_t>(x);
    return true;
}
void build_explicit_quad_432af0(Input d, std::span<PolygonSubmissionVertex, 4> vertices)
{
    const auto matrix = (d[0] & 1) ? rotation(d) : VehicleMatrix{};
    const auto color = (d[0] & 0x20) ? d[18] : 0xffffffffu;
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto at = 19 + i * 4;
        auto &out = vertices[i].words;
        float x = as_float(d[at]), y = as_float(d[at + 1]);
        if (d[0] & 1)
            rotate(x, y, d, matrix);
        if (d[0] & 8)
            scale(x, y, d);
        out[0] = word(double(as_float(d[1])) + x);
        out[1] = word(double(as_float(d[2])) + y);
        out[2] = out[3] = d[3];
        out[4] = color;
        out[5] = 0;
        out[6] = word(double(as_float(d[at + 2])) * 0.0009765625);
        out[7] = word(double(as_float(d[at + 3])) * 0.0009765625);
    }
}
void build_rectangle_quad_432c00(Input d, std::span<PolygonSubmissionVertex, 4> vertices)
{
    const auto matrix = (d[0] & 1) ? rotation(d) : VehicleMatrix{};
    const auto color = (d[0] & 0x20) ? d[18] : 0xffffffffu;
    const auto u0 = word((double(as_float(d[6])) + 1.0) * 0.0009765625),
               v0 = word((double(as_float(d[7])) + 1.0) * 0.0009765625);
    const auto u1 = word((double(as_float(d[8])) - 1.0) * 0.0009765625),
               v1 = word((double(as_float(d[9])) - 1.0) * 0.0009765625);
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &out = vertices[i].words;
        float x = (i == 1 || i == 2) ? as_float(d[4]) : 0.0f, y = i >= 2 ? as_float(d[5]) : 0.0f;
        if (d[0] & 1)
            rotate(x, y, d, matrix);
        if (d[0] & 8)
            scale(x, y, d);
        out[0] = word(double(as_float(d[1])) + x);
        out[1] = word(double(as_float(d[2])) + y);
        out[2] = out[3] = d[3];
        out[4] = color;
        out[5] = 0;
        out[6] = (i == 1 || i == 2) ? u1 : u0;
        out[7] = i >= 2 ? v1 : v0;
    }
}
void submit_quad_432e00(QuadSubmission frame, PolygonSubmissionSink &sink)
{
    std::array<std::uint32_t, 35> local;
    std::copy(frame.descriptor.begin(), frame.descriptor.end(), local.begin());
    // NEW HOST (wide_hud R20): anchored overlay mapping. Identity (the
    // viewport's own floats and clip) outside an anchor scope or on a 4:3 canvas.
    const auto map = host_wide_overlay::overlay_mapping(frame.viewport);
    if (!(local[0] & 0x40))
    {
        if (map.anchored)
        {
            ViewportState widened = frame.viewport;
            widened.clip_virtual = map.clip; // host-local copy
            if (!clip_quad_4251a0(local, widened))
                return;
        }
        else if (!clip_quad_4251a0(local, frame.viewport))
            return;
    }
    if (local[0] & 4)
        build_explicit_quad_432af0(local, frame.vertices);
    else
        build_rectangle_quad_432c00(local, frame.vertices);
    apply_screen_polygon_blend_432190(local[11], sink);
    const bool alpha = (local[11] & 0x40) != 0;
    sink.set_render_state(24, alpha ? 1u : 160u);
    const auto id = local[10];
    if (frame.selected_material != id)
    {
        require(id < frame.materials.size(), "quad material outside live source namespace");
        sink.set_texture(0, frame.materials[id].primary_texture);
        frame.selected_material = id;
    }
    for (auto &vertex : frame.vertices)
    {
        auto &v = vertex.words;
        v[0] = word((double(as_float(v[0])) - 320.0) * map.scale_x + map.center_x);
        v[1] = word((double(as_float(v[1])) - 240.0) * map.scale_y + map.center_y);
        if (alpha)
            v[4] = (v[4] & 0xffffffu) | (local[12] << 24);
        v[5] |= 0xff000000u;
    }
    sink.draw(6, 0x2c4, frame.vertices, 0x14, frame.viewport);
    if (alpha)
        sink.set_render_state(27, 0);
}
} // namespace sgn::retail
