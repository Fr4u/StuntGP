#include "sgn/retail_polygon_submission.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <limits>

namespace sgn::retail
{
namespace
{
using Route = PolygonSubmissionRoute;
using sgn::format_check::require;
std::uint32_t add_float(std::uint32_t a, float b)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(double(std::bit_cast<float>(a)) + double(b)));
}
const PolygonSubmissionMaterial &material(const PolygonSubmissionInputs &in, std::uint32_t id)
{
    require(id < in.materials.size(), "polygon submission global material identity outside owner");
    return in.materials[id];
}
void depth(const PolygonSubmissionInputs &in, PolygonSubmissionState &state, PolygonSubmissionSink &sink)
{
    // 432A90..432AE2: capability-absent path performs no writes or calls.
    if (!in.depth_capability)
        return;
    sink.set_render_state(23, 7);
    sink.set_render_state(7, 1);
    state.depth_scale_bits = 0x43000000;
    state.depth_enabled = 1;
    state.depth_write = 1;
    state.depth_other = 0;
}
void specular(bool enable, const PolygonSubmissionInputs &in, PolygonSubmissionState &state,
              PolygonSubmissionSink &sink)
{
    // 42F340/42F380 write592050 BEFORE the first COM call.
    if (in.legacy_driver_restriction == 1)
        return;
    state.specular_enabled = enable ? 1u : 0u;
    sink.set_render_state(29, state.specular_enabled);
    sink.set_render_state(24, enable ? 1u : 160u);
}
std::size_t count(const WorldPolygonPacket &packet)
{
    const auto n = packet.header.byte(0);
    require(n >= 3, "polygon submission requires valid source polygon count");
    require(n <= packet.indices.size(), "polygon submission corner list outside owner");
    return n;
}
WorldPolygonAttribute attribute(const WorldPolygonPacket &p, std::size_t corner)
{
    return p.attributes.value(p.indices[corner]);
}
void vertex(PolygonSubmissionVertex &out, const WorldPolygonPacket &p, const WorldPolygonAttribute &a, Route route,
            float scroll_u, float scroll_v, std::uint32_t mode)
{
    require(a.vertex < p.vertices.size(), "polygon submission transformed vertex outside owner");
    const auto &v = p.vertices[a.vertex].words;
    auto &w = out.words;
    w[0] = v[0];
    w[1] = v[1];
    w[2] = v[7];
    w[3] = v[7];
    if (route == Route::multitexture)
    {
        w[6] = a.u;
        w[7] = a.v;
        w[8] = v[5];
        w[9] = v[6];
    }
    else
    {
        w[6] = add_float(a.u, scroll_u);
        w[7] = add_float(a.v, scroll_v);
    }
    // Only433C50 writes the second UV pair. Other routes retain both words.
    if (route == Route::sky)
    {
        w[4] = 0xffffffffu;
        w[5] = 0;
    }
    else if (route == Route::opaque)
    {
        w[4] = a.color | 0xff000000u;
        w[5] = 0;
    }
    else if (route == Route::alpha)
    {
        w[4] = mode == 0x50 ? a.color : (a.color & 0xffffffu) | (std::uint32_t(p.header.byte(2)) << 24);
        w[5] = 0;
    }
    else
    {
        w[4] = a.color;
        const auto alpha = a.color >> 24;
        w[5] = (alpha << 16) | (alpha << 8) | alpha;
    }
}
std::array<const float *, 2> motion(const PolygonSubmissionInputs &in, std::uint32_t id, Route route)
{
    if (route == Route::multitexture)
        return {};
    if (route == Route::sky)
    {
        const auto index = id - in.sky_material_base;
        require(index < in.sky.size(), "polygon submission sky material outside two source motion records");
        return {&in.sky[index].u, &in.sky[index].v};
    }
    const auto index = material(in, id).motion_index;
    require(index < in.motions.size(), "polygon submission motion index outside live track bank");
    return {&in.motions[index].u, &in.motions[index].v};
}
void indexed(Route route, std::span<const WorldPolygonPacket> packets, const PolygonSubmissionInputs &in,
             PolygonSubmissionState &state, PolygonSubmissionSink &sink)
{
    if (packets.empty())
        return;
    if (route == Route::multitexture && (!in.multitexture_capability || !in.quality))
    {
        indexed(Route::opaque, packets, in, state, sink);
        return;
    }
    if (route == Route::sky)
    {
        const auto result = sink.set_render_state(23, 8);
        require(result == 0,
                "original sky ZFUNC failed; legacy422690/44E4A0 error termination is outside submission domain");
    }
    if ((route == Route::specular || route == Route::multitexture) && in.quality == 1)
        specular(true, in, state, sink);
    if (route == Route::multitexture)
    {
        sink.set_texture(1, material(in, 0).alternate_texture);
        sink.set_texture_stage_state(1, 11, 1);
        sink.set_texture_stage_state(1, 1, 7);
    }
    std::uint32_t current = std::numeric_limits<std::uint32_t>::max();
    std::size_t vertices = 0, indices = 0;
    std::array<const float *, 2> scroll{};
    auto flush = [&] {
        if (vertices)
            sink.draw_indexed(4, 0x2c4, std::span(state.indexed_vertices).first(vertices),
                              std::span(state.indices).first(indices), 0x1c, in.viewport);
    };
    for (const auto &p : packets)
    {
        if (!p.header)
            continue;
        const auto id = std::uint32_t(p.header.byte(1));
        if (id != current)
        {
            flush();
            vertices = 0;
            indices = 0;
            current = id;
            sink.set_texture(0, material(in, id).primary_texture);
            scroll = motion(in, id, route);
        }
        const auto n = count(p);
        require(vertices <= state.indexed_vertices.size() && n <= state.indexed_vertices.size() - vertices,
                "polygon submission vertex scratch capacity exceeded");
        require(indices <= state.indices.size() && (n - 2) * 3 <= state.indices.size() - indices,
                "polygon submission index scratch capacity exceeded");
        require(vertices + n <= 65536, "polygon submission WORD index capacity exceeded");
        for (std::size_t i = 0; i < n; ++i)
            vertex(state.indexed_vertices[vertices + i], p, attribute(p, i), route, scroll[0] ? *scroll[0] : 0,
                   scroll[1] ? *scroll[1] : 0, 0);
        for (std::size_t i = 1; i + 1 < n; ++i)
        {
            state.indices[indices++] = static_cast<std::uint16_t>(vertices);
            state.indices[indices++] = static_cast<std::uint16_t>(vertices + i);
            state.indices[indices++] = static_cast<std::uint16_t>(vertices + i + 1);
        }
        vertices += n;
    }
    flush();
    if (route == Route::multitexture)
        sink.set_texture_stage_state(1, 1, 1);
    if ((route == Route::specular || route == Route::multitexture) && in.quality == 1)
        specular(false, in, state, sink);
    if (route == Route::sky)
        depth(in, state, sink);
}
// Raw pinned477CD0 rows3..15. No fan substitution or cross-polygon sort.
constexpr std::array<std::array<std::uint8_t, 15>, 13> strip{{{{1, 0, 2}},
                                                              {{1, 0, 2, 3}},
                                                              {{1, 0, 2, 4, 3}},
                                                              {{1, 0, 2, 5, 3, 4}},
                                                              {{1, 0, 2, 6, 3, 5, 4}},
                                                              {{1, 0, 2, 7, 3, 6, 4, 5}},
                                                              {{1, 0, 2, 8, 3, 7, 4, 6, 5}},
                                                              {{1, 0, 2, 9, 3, 8, 4, 7, 5, 6}},
                                                              {{1, 0, 2, 10, 3, 9, 4, 8, 5, 7, 6}},
                                                              {{1, 0, 2, 11, 3, 10, 4, 9, 5, 8, 6, 7}},
                                                              {{1, 0, 2, 12, 3, 11, 4, 10, 5, 9, 6, 8, 7}},
                                                              {{1, 0, 2, 13, 3, 12, 4, 11, 5, 10, 6, 9, 7, 8}},
                                                              {{1, 0, 2, 14, 3, 13, 4, 12, 5, 11, 6, 10, 7, 9, 8}}}};
void alpha(std::span<const WorldPolygonPacket> packets, const PolygonSubmissionInputs &in,
           PolygonSubmissionState &state, PolygonSubmissionSink &sink)
{
    if (packets.empty())
        return;
    sink.set_render_state(14, 0);
    sink.set_render_state(27, 1);
    std::uint32_t current_material = 0xffffffffu, current_mode = 0xffffffffu;
    std::array<const float *, 2> scroll{};
    for (const auto &p : packets)
    {
        if (!p.header)
            continue;
        const auto mode = std::uint32_t(p.header.byte(3) & 0x78);
        if (mode != current_mode)
        {
            current_mode = mode;
            const auto source = mode == 0x70 ? 9u : mode == 0x58 ? 2u : 5u;
            const auto destination = mode == 0x70 ? 1u : (mode == 0x48 || mode == 0x50) ? 6u : 2u;
            sink.set_render_state(19, source);
            sink.set_render_state(20, destination);
            sink.set_texture_stage_state(0, 2, 2);
        }
        const auto id = std::uint32_t(p.header.byte(1));
        if (id != current_material)
        {
            current_material = id;
            sink.set_texture(0, material(in, id).primary_texture);
            scroll = motion(in, id, Route::alpha);
        }
        const auto n = count(p);
        require(n <= 15, "polygon submission alpha count outside pinned strip rows");
        require(n <= state.alpha_vertices.size(), "polygon submission alpha scratch capacity exceeded");
        for (std::size_t i = 0; i < n; ++i)
            vertex(state.alpha_vertices[i], p, attribute(p, strip[n - 3][i]), Route::alpha, *scroll[0], *scroll[1],
                   mode);
        sink.draw(5, 0x2c4, std::span(state.alpha_vertices).first(n), 0x1c, in.viewport);
    }
    sink.set_render_state(19, 2);
    sink.set_render_state(20, 1);
    sink.set_texture_stage_state(0, 2, 2);
    sink.set_texture_stage_state(0, 3, 0);
    sink.set_render_state(27, 0);
    sink.set_render_state(14, 1);
    depth(in, state, sink);
}
} // namespace
void submit_polygon_route(PolygonSubmissionRoute route, std::span<const WorldPolygonPacket> packets,
                          const PolygonSubmissionInputs &in, PolygonSubmissionState &state, PolygonSubmissionSink &sink)
{
    switch (route)
    {
    case Route::sky:
    case Route::opaque:
    case Route::specular:
    case Route::multitexture:
        indexed(route, packets, in, state, sink);
        break;
    case Route::alpha:
        alpha(packets, in, state, sink);
        break;
    default:
        throw FormatError("unknown polygon submission route");
    }
}
void submit_world_polygons(const PolygonSubmissionLists &lists, const PolygonSubmissionInputs &in,
                           PolygonSubmissionState &state, PolygonSubmissionSink &sink)
{
    depth(in, state, sink);
    sink.set_render_state(24, 160);
    indexed(Route::sky, lists.categories[0], in, state, sink);
    indexed(Route::opaque, lists.categories[1], in, state, sink);
    indexed(Route::opaque, lists.categories[2], in, state, sink);
    // 433F8A reads624984 once for this pair; each helper has its own live gates.
    const auto middle = in.quality == 1 ? Route::specular : Route::opaque;
    indexed(middle, lists.categories[3], in, state, sink);
    indexed(middle, lists.categories[4], in, state, sink);
    indexed(Route::multitexture, lists.categories[5], in, state, sink);
    indexed(Route::multitexture, lists.categories[6], in, state, sink);
    sink.set_render_state(24, 1);
    alpha(lists.alpha[0], in, state, sink);
    alpha(lists.alpha[1], in, state, sink);
}
} // namespace sgn::retail
