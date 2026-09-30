#include "sgn/retail_trail_render.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
std::uint32_t integer(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("trail alpha integer range");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
unsigned segments(const TrailRenderProfile &p, unsigned count)
{
    if (count <= 1)
        return 0;
    if (count <= p.stride)
        return 1;
    return count / p.stride + (count < p.capacity ? 1u : 0u);
}
void validate(TrailRenderBindings b, const TrailRenderProfile &p, std::span<const VehicleVector> ring, unsigned cursor,
              unsigned count, float scale, float alpha, std::span<const bool> tags)
{
    if (!p.capacity || p.capacity >= ring.size() || cursor >= p.capacity || count > p.capacity || !p.stride ||
        p.stride > p.capacity || p.max_segments > 15 || segments(p, count) > 15)
        throw FormatError("trail ring/profile bounds");
    if (!tags.empty() && tags.size() != ring.size())
        throw FormatError("trail ring audit bounds");
    if (!std::isfinite(scale) || !std::isfinite(alpha))
        throw FormatError("nonfinite trail scale");
    for (float value : p.width)
        if (!std::isfinite(value))
            throw FormatError("nonfinite trail width");
    for (float value : p.head_scale)
        if (!std::isfinite(value))
            throw FormatError("nonfinite trail head scale");
    for (float value : p.uv)
        if (!std::isfinite(value))
            throw FormatError("nonfinite trail UV");
    for (float value : p.alpha)
        (void)integer(double(alpha) * value);
    (void)integer(double(p.head_alpha) * alpha);
    for (const auto &v : ring)
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
            throw FormatError("nonfinite trail point");
    if (p.shape_identity)
    {
        const auto &shape = trail_shape(b.profiles, p.shape_identity);
        for (const auto &v : shape)
            if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
                throw FormatError("nonfinite trail shape");
    }
    // Screen audit capacity is checked at each actual append after425838.
    // World-only trails do not read the screen publication.
}
void publish_screen(TrailRenderBindings b, SourceScreenPolygon &polygon, ScreenPolygonAudit &audit)
{
    append_screen_polygon(b.screen, polygon, b.viewport, &audit,
                          b.audit ? b.audit->screen : std::span<ScreenPolygonAudit>{});
}
ScreenPolygonAudit screen_template(SourceScreenPolygon &polygon, const TrailRenderProfile &p, bool head)
{
    polygon.texture = head ? p.head_texture : p.texture;
    polygon.flags = head ? p.head_flags : p.flags;
    polygon.vertex_count = 4;
    ScreenPolygonAudit audit;
    audit.undefined.fill(true);
    for (unsigned j = 0; j < 4; ++j)
        audit.undefined[j] = false;
    for (unsigned j = 0; j < 4; ++j)
        for (unsigned k : {0u, 1u, 2u, 4u, 5u, 6u})
            audit.undefined[4 + j * 8 + k] = false;
    return audit;
}
void screen_head(TrailRenderBindings b, const TrailRenderProfile &p, VehicleVector point, float scale)
{ // 41EE10
    if (point.z < 0 || point.z > 1)
        return;
    SourceScreenPolygon polygon{};
    auto audit = screen_template(polygon, p, true);
    polygon.alpha = p.head_alpha;
    const float half_x = f32((double(scale) * p.head_scale[0]) * point.z * 100),
                half_y = f32((double(scale) * p.head_scale[1]) * point.z * 100);
    const float left = f32(double(point.x) - half_x), right = f32(double(half_x) + point.x),
                top = f32(double(point.y) - half_y), bottom = f32(double(half_y) + point.y);
    const std::array<float, 4> x{left, right, right, left}, y{top, top, bottom, bottom}, u{0, 1, 1, 0}, v{0, 0, 1, 1};
    for (unsigned j = 0; j < 4; ++j)
    {
        auto &dst = polygon.vertices[j];
        dst.x = x[j];
        dst.y = y[j];
        dst.z = point.z;
        dst.u = u[j];
        dst.v = v[j];
        dst.color = p.head_color;
    }
    publish_screen(b, polygon, audit);
}
void screen_trail(TrailRenderBindings b, const TrailRenderProfile &p, std::span<const VehicleVector> ring,
                  unsigned cursor, unsigned count, float scale, float alpha)
{ // 41EF90
    if (p.head_enabled == 1)
        screen_head(b, p, ring[cursor], scale);
    SourceScreenPolygon polygon{};
    auto audit = screen_template(polygon, p, false);
    auto point = ring[cursor];
    double old_width = double(scale) * p.width[0];
    unsigned traversed = 0;
    VehicleVector previous_left{}, previous_right{};
    for (unsigned j = 0; j < p.max_segments; ++j)
    {
        const float width = f32(double(scale) * p.width[j + 1]);
        polygon.alpha = integer(double(alpha) * p.alpha[j]);
        traversed += p.stride;
        if (traversed >= count)
            break;
        const auto raw = std::bit_cast<std::int32_t>(cursor - p.stride);
        cursor = raw < 0 ? static_cast<unsigned>(raw) + p.capacity : static_cast<unsigned>(raw);
        const auto previous = point;
        point = ring[cursor];
        if (j == 0)
        {
            const float radius = f32(double(point.z) * old_width * 100);
            auto direction =
                vehicle_normalize({f32(double(point.x) - previous.x), f32(double(point.y) - previous.y), 0, 0});
            const double dx = double(direction.y) * radius;
            const float dy = f32(double(-direction.x) * radius);
            previous_left = {f32(double(previous.x) - dx), f32(double(previous.y) - dy), previous.z, 0};
            previous_right = {f32(dx + previous.x), f32(double(dy) + previous.y), previous.z, 0};
        }
        const float radius = f32(double(point.z) * width * 100);
        auto direction =
            vehicle_normalize({f32(double(point.x) - previous.x), f32(double(point.y) - previous.y), 0, 0});
        const float dx = f32(double(direction.y) * radius), dy = f32(double(-direction.x) * radius);
        const VehicleVector next_left{f32(double(point.x) - dx), f32(double(point.y) - dy), point.z, 0};
        const VehicleVector next_right{f32(double(dx) + point.x), f32(double(dy) + point.y), point.z, 0};
        const std::array<VehicleVector, 4> points{previous_left, next_left, next_right, previous_right};
        const std::array<float, 4> u{p.uv[0], p.uv[2], p.uv[2], p.uv[0]}, v{p.uv[1], p.uv[1], p.uv[3], p.uv[3]};
        for (unsigned k = 0; k < 4; ++k)
        {
            auto &dst = polygon.vertices[k];
            dst.x = points[k].x;
            dst.y = points[k].y;
            dst.z = points[k].z;
            dst.u = u[k];
            dst.v = v[k];
            dst.color = (k == 0 || k == 3) ? p.color[j] : p.color[j + 1];
        }
        publish_screen(b, polygon, audit);
        previous_left = next_left;
        previous_right = next_right;
        old_width = width;
    }
}
VehicleVector world_point(const VehicleVector &point, float width, const VehicleVector &shape, const VehicleMatrix &m)
{
    const double x = double(width) * shape.x, y = double(width) * shape.y, z = double(width) * shape.z;
    const float tx = f32((x * m[0] + z * m[8]) + y * m[4]), ty = f32((z * m[9] + y * m[5]) + x * m[1]);
    const double tz = (x * m[2] + z * m[10]) + y * m[6];
    return {f32(double(tx) + point.x), f32(double(ty) + point.y), f32(tz + point.z), 0};
}
void world_segment(TrailRenderBindings b, const TrailRenderProfile &p, const VehicleVector &previous,
                   const VehicleVector &next, float old_width, float width, unsigned segment)
{ // 41E760/41EA20
    const auto &shape = trail_shape(b.profiles, p.shape_identity);
    std::array<VehicleVector, 8> points;
    for (unsigned j = 0; j < 4; ++j)
    {
        points[j] = world_point(previous, old_width, shape[j], b.current_vehicle_orientation);
        points[j + 4] = world_point(next, width, shape[j], b.current_vehicle_orientation);
    }
    auto &staging = b.staging;
    staging.vertices[0].color = p.color[segment];
    staging.vertices[1].color = p.color[segment];
    staging.vertices[2].color = p.color[segment + 1];
    staging.vertices[3].color = p.color[segment + 1];
    constexpr std::array<std::array<unsigned, 4>, 2> cross{{{0, 1, 5, 4}, {2, 3, 7, 6}}};
    constexpr std::array<std::array<unsigned, 4>, 4> tube{{{1, 2, 6, 5}, {0, 3, 7, 4}, {0, 1, 5, 4}, {3, 2, 6, 7}}};
    const auto quads =
        p.mode == 0 ? std::span<const std::array<unsigned, 4>>(cross) : std::span<const std::array<unsigned, 4>>(tube);
    for (const auto &quad : quads)
    {
        for (unsigned j = 0; j < 4; ++j)
        {
            auto &dst = staging.vertices[j].position;
            const auto &src = points[quad[j]];
            dst.x = src.x;
            dst.y = src.y;
            dst.z = src.z;
            if (b.audit)
                b.audit->packet.staging_w[j] = true;
        }
        append_trail_quad_packet(b.sprites, staging, b.audit ? &b.audit->packet : nullptr);
    }
}
} // namespace
void append_retail_trail(TrailRenderBindings b, const TrailRenderProfile &p, std::span<const VehicleVector> ring,
                         unsigned cursor, unsigned count, float scale, float alpha, std::span<const bool> tags)
{
    validate(b, p, ring, cursor, count, scale, alpha, tags);
    auto &staging = b.staging;
    if (p.head_enabled == 1 && p.head_scale[2] != 0)
    {
        staging.scale.x = f32(double(scale) * p.head_scale[0]);
        staging.scale.y = f32(double(scale) * p.head_scale[1]);
        staging.scale.z = f32(double(scale) * p.head_scale[2]);
        for (auto &vertex : staging.vertices)
            vertex.color = p.head_color;
        staging.position = std::bit_cast<CameraVector>(ring[p.capacity]);
        if (b.audit)
            b.audit->staging_position_w = !tags.empty() && tags[p.capacity];
        staging.alpha = integer(double(p.head_alpha) * alpha);
        staging.texture = p.head_texture;
        staging.flags = p.head_flags;
        append_trail_billboard_packet(b.sprites, staging, b.camera);
    }
    if (b.split_setting != 0)
        return;
    if (!p.shape_identity)
    {
        screen_trail(b, p, ring, cursor, count, scale, alpha);
        return;
    }
    float old_width = f32(double(scale) * p.width[0]);
    staging.texture = p.texture;
    staging.flags = p.flags;
    staging.vertex_count = 4;
    const auto length = segments(p, count);
    auto previous = ring[cursor];
    auto running = std::bit_cast<std::int32_t>(cursor);
    bool first = true;
    for (unsigned j = 0; j < length; ++j)
    {
        const float width = f32(double(scale) * p.width[j + 1]);
        staging.alpha = integer(double(alpha) * p.alpha[j]);
        running = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(running) - p.stride);
        std::int32_t selected = running;
        if (count < p.capacity)
        {
            if (selected < 1)
                selected = 1;
        }
        else if (selected < 0)
            selected = std::bit_cast<std::int32_t>(p.capacity + static_cast<unsigned>(selected));
        if (selected < 0 || static_cast<unsigned>(selected) >= ring.size())
            throw FormatError("trail source ring index");
        const auto next = ring[static_cast<unsigned>(selected)];
        if (p.mode <= 1)
        {
            if (first)
            {
                const std::array<float, 4> u{p.uv[0], p.uv[2], p.uv[2], p.uv[0]}, v{p.uv[1], p.uv[1], p.uv[3], p.uv[3]};
                for (unsigned k = 0; k < 4; ++k)
                {
                    staging.vertices[k].u = u[k];
                    staging.vertices[k].v = v[k];
                }
                first = false;
            }
            world_segment(b, p, previous, next, old_width, width, j);
        }
        previous = next;
        old_width = width;
    }
}
void append_impact_particle_trails(TrailRenderBindings b, const contact::effects::ImpactParticles &particles,
                                   const contact::effects::ImpactParticleDrawList &list,
                                   const contact::effects::ImpactParticleLifetimeAudit *audit)
{
    // 416055..416061: unsigned cursor<=listbase returns before any slot,
    //  particle, profile or trail-span access. Normalized null is source0.
    if (list.cursor_is_null())
        return;
    if (list.count > list.slots.size())
        throw FormatError("impact trail publication count");
    for (unsigned j = list.count; j > 0; --j)
    {
        const auto slot = list.slots[j - 1]; // 416067: reverse publication order
        // Reject at the reached dereference, retaining earlier valid appends.
        if (slot >= particles.particles.size())
            throw FormatError("impact trail publication identity");
        const auto &p = particles.particles[slot];
        if (p.type < 0 || static_cast<unsigned>(p.type) >= b.profiles.profiles.size())
            throw FormatError("impact trail type");
        const double factor =
            (p.lifetime > 15 ? 1 : double(p.lifetime) * std::bit_cast<float>(std::uint32_t{0x3d888889})) * p.scale;
        float scale = f32(factor);
        if (factor < .25)
            scale = .25f;
        else if (scale > 1)
            scale = 1;
        const auto ring = std::bit_cast<std::array<VehicleVector, 64>>(p.reserved);
        append_retail_trail(b, b.profiles.profiles[static_cast<unsigned>(p.type)], ring, p.field_430, p.field_434,
                            scale, scale,
                            audit ? std::span<const bool>(audit->trail_position[slot]) : std::span<const bool>{});
    }
}
} // namespace sgn::retail
