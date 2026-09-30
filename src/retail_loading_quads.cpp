#include "sgn/retail_loading_quads.hpp"
#include "sgn/retail_loading_tables.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using Descriptor = std::array<std::uint32_t, 35>;
using sgn::bit_reinterpret::as_float;
std::uint32_t word(double value)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}
using sgn::bit_reinterpret::as_signed;
bool below(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
bool clip(Descriptor &d, const ViewportState &viewport)
{
    // 4251A0 flags8-clear path. CMP at425216/22F/248/261 reads signed IEEE
    // bits after FSTP, not a numeric float-to-int conversion.
    float x = as_float(d[1]), y = as_float(d[2]);
    double width = as_float(d[4]), height = as_float(d[5]);
    const float right = static_cast<float>(double(x) + width), bottom = static_cast<float>(double(y) + height);
    const auto &c = viewport.clip_virtual;
    if (as_signed(word(double(right) - c[0])) <= 0 || as_signed(word(double(x) - c[1])) >= 0 ||
        as_signed(word(double(bottom) - c[2])) <= 0 || as_signed(word(double(y) - c[3])) >= 0)
        return false;
    if (!below(x, c[0]) && below(right, c[1]) && !below(y, c[2]) && below(bottom, c[3]))
        return true;
    double u0 = as_float(d[6]), v0 = as_float(d[7]), u1 = as_float(d[8]), v1 = as_float(d[9]);
    if (below(x, c[0]))
    {
        const float delta = static_cast<float>(double(c[0]) - x); // 4252E1 local FSTP
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
void rectangle(const Descriptor &d, std::span<PolygonSubmissionVertex, 4> vertices)
{
    // 432C00 flags1/8-clear path. The second UV pair is never written.
    const auto u0 = word((double(as_float(d[6])) + 1.0) * 0.0009765625),
               v0 = word((double(as_float(d[7])) + 1.0) * 0.0009765625);
    const auto u1 = word((double(as_float(d[8])) - 1.0) * 0.0009765625),
               v1 = word((double(as_float(d[9])) - 1.0) * 0.0009765625);
    const auto color = (d[0] & 0x20) ? d[18] : 0xffffffffu;
    for (std::size_t i = 0; i < 4; ++i)
    {
        auto &v = vertices[i].words;
        const float x = (i == 1 || i == 2) ? as_float(d[4]) : 0.0f, y = i >= 2 ? as_float(d[5]) : 0.0f;
        v[0] = word(double(as_float(d[1])) + x);
        v[1] = word(double(as_float(d[2])) + y);
        v[2] = v[3] = d[3];
        v[4] = color;
        v[5] = 0;
        v[6] = (i == 1 || i == 2) ? u1 : u0;
        v[7] = i >= 2 ? v1 : v0;
    }
}
void blend(std::uint32_t flags, PolygonSubmissionSink &sink)
{
    // 432190 source jump table, loading callers20 and48 only.
    if (flags == 0x20)
    {
        sink.set_render_state(27, 0);
        sink.set_render_state(19, 2);
        sink.set_render_state(20, 1);
    }
    else
    {
        sink.set_render_state(27, 1);
        sink.set_render_state(19, 5);
        sink.set_render_state(20, 6);
    }
    sink.set_texture_stage_state(0, 2, 2);
}
void submit(LoadingQuadWorld world, PolygonSubmissionSink &sink)
{
    Descriptor local;
    std::copy(world.descriptor.begin(), world.descriptor.end(), local.begin());
    if (local[0] != 2 || (local[11] != 0x20 && local[11] != 0x48))
        throw FormatError("loading rectangle outside source caller flags");
    // NEW HOST (wide_hud R20): anchored mapping; identity outside a scope or on 4:3.
    const auto map = host_wide_overlay::overlay_mapping(world.viewport);
    if (map.anchored)
    {
        ViewportState widened = world.viewport;
        widened.clip_virtual = map.clip; // host-local copy
        if (!clip(local, widened))
            return;
    }
    else if (!clip(local, world.viewport))
        return;
    const auto material = local[10];
    if (material >= world.materials.size())
        throw FormatError("loading material identity outside live owner");
    rectangle(local, world.vertices);
    blend(local[11], sink);
    const bool alpha = (local[11] & 0x40) != 0;
    sink.set_render_state(24, alpha ? 1u : 160u);
    if (world.selected_material != material)
    {
        sink.set_texture(0, world.materials[material].primary_texture);
        world.selected_material = material;
    }
    for (auto &vertex : world.vertices)
    {
        auto &v = vertex.words;
        v[0] = word((double(as_float(v[0])) - 320.0) * map.scale_x + map.center_x);
        v[1] = word((double(as_float(v[1])) - 240.0) * map.scale_y + map.center_y);
        if (alpha)
            v[4] = (v[4] & 0xffffffu) | (local[12] << 24);
        v[5] |= 0xff000000u;
    }
    sink.draw(6, 0x2c4, world.vertices, 0x14, world.viewport);
    if (alpha)
        sink.set_render_state(27, 0);
}
const ControllerOverlayTransition &current(LoadingQuadWorld world)
{
    if (world.controls.current >= 4)
        throw FormatError("loading overlay controller outside live owner");
    return world.overlays[world.controls.current];
}
} // namespace
void draw_loading_tiles_445800(LoadingQuadWorld world, PolygonSubmissionSink &sink)
{
    auto d = world.descriptor;
    std::fill(d.begin(), d.end(), 0u);
    d[0] = 2;
    d[3] = 0x3f70a3d7;
    d[6] = d[7] = 0x3f800000;
    d[8] = d[9] = 0x447f0000;
    d[11] = 0x20;
    auto material = world.tile_material_base;
    for (const auto &tile : source_loading_tiles())
    {
        d[1] = word(double(as_float(tile[0])) + 64.0);
        d[4] = tile[2];
        d[2] = tile[1];
        d[5] = tile[3];
        d[10] = material;
        submit(world, sink);
        ++material;
    }
}
void draw_loading_overlay_4428a0(LoadingQuadWorld world, PolygonSubmissionSink &sink)
{
    if (world.pause_a == 1 && world.pause_b == 1)
        return;
    const auto &overlay = current(world);
    if (overlay.active == 0 || as_signed(overlay.initial_value) < 1)
        return;
    const auto product = overlay.initial_value * 265u;
    const auto quotient = static_cast<std::uint32_t>(as_signed(product) / 100);
    // NEW HOST (wide_hud R20): the flat 640x480 transition fade covers the widened view.
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::fade);
    auto d = world.descriptor;
    d[0] = 2;
    d[1] = d[2] = 0;
    d[3] = std::bit_cast<std::uint32_t>(overlay.rate);
    d[4] = 0x44200000;
    d[5] = 0x43f00000;
    d[11] = 0x48;
    d[12] = std::min(quotient, 255u);
    if (overlay.kind == 1 || overlay.kind == 2)
    {
        d[6] = d[8] = overlay.kind == 1 ? 0x43c00000 : 0x43000000;
        d[7] = d[9] = 0x43000000;
        d[10] = world.loading_fade == 1 ? 0u : 10u;
    }
    submit(world, sink);
}
void draw_loading_overlay_before_442a00(LoadingQuadWorld world, PolygonSubmissionSink &sink)
{
    const auto &overlay = current(world);
    if (overlay.active == 1 && std::bit_cast<std::uint32_t>(overlay.rate) != 0x3f800000)
        draw_loading_overlay_4428a0(world, sink);
}
void draw_loading_overlay_after_442a20(LoadingQuadWorld world, PolygonSubmissionSink &sink)
{
    const auto &overlay = current(world);
    if (overlay.active == 1 && std::bit_cast<std::uint32_t>(overlay.rate) == 0x3f800000)
        draw_loading_overlay_4428a0(world, sink);
}
} // namespace sgn::retail
