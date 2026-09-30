#include "sgn/retail_trail_packets.hpp"
#include "sgn/source_trail_packet_writer.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
void space(const SourceSpritePacketStorage &p, unsigned count)
{
    if (p.index_count > 2048 - count || p.position_count > 2048 - count || p.attribute_count > 2048 - count)
        throw FormatError("trail packet stream capacity");
}
void header(SourceSpritePacketStorage &p, const SourceTrailPolygon &s, unsigned count)
{
    ++p.polygons;
    p.vertices = static_cast<std::uint16_t>(p.vertices + count);
    p.corners = static_cast<std::uint16_t>(p.corners + count);
    auto &h = p.headers[p.header_bytes / 8];
    h[0] = static_cast<std::uint8_t>(count);
    h[1] = static_cast<std::uint8_t>(s.texture);
    h[2] = static_cast<std::uint8_t>(s.alpha);
    h[3] = static_cast<std::uint8_t>(s.flags | 0x80);
    h[6] = static_cast<std::uint8_t>(p.index_count);
    h[7] = static_cast<std::uint8_t>(p.index_count >> 8);
    p.header_bytes += 8;
}
void attribute(SourceSpritePacketStorage &p, std::uint32_t u, std::uint32_t v, std::uint32_t color)
{
    auto &a = p.attributes[p.attribute_count];
    a.u = u;
    a.v = v;
    a.color = color;
    a.vertex = static_cast<std::uint16_t>(p.position_count);
    p.indices[p.index_count++] = static_cast<std::uint16_t>(p.attribute_count++);
}
} // namespace
bool append_trail_quad_packet(SourceSpritePacketStorage &p, const SourceTrailPolygon &s, TrailPacketAudit *audit)
{
    if (p.source_part.source_bound())
    {
        if (audit)
            throw SourceAllocationError(0x4342e0, "preview trail audit is not a source W owner");
        return append_source_trail_quad_4342e0(p, s);
    }
    (void)sprite_packet_view(p);
    if (p.header_bytes >= 4096)
        return false;
    if (s.vertex_count > 4)
        throw FormatError("trail staging vertex count");
    space(p, s.vertex_count);
    header(p, s, s.vertex_count);
    for (unsigned j = 0; j < s.vertex_count; ++j)
    {
        const auto &src = s.vertices[j];
        attribute(p, std::bit_cast<std::uint32_t>(src.u), std::bit_cast<std::uint32_t>(src.v), src.color);
        auto &dst = p.positions[p.position_count];
        const auto old_w = dst.w;
        dst = src.position;
        if (audit)
        {
            if (audit->staging_w[j])
                dst.w = old_w;
            audit->packet_w[p.position_count] = audit->staging_w[j];
        }
        ++p.position_count;
    }
    return true;
}
bool append_trail_billboard_packet(SourceSpritePacketStorage &p, const SourceTrailPolygon &s, LightCameraView view)
{
    if (p.source_part.source_bound())
        return append_source_trail_billboard_4343f0(p, s, view);
    (void)sprite_packet_view(p);
    if (p.header_bytes >= 4096)
        return false;
    SourceSpriteRecord classifier;
    classifier.position = s.position;
    classifier.scale = {s.scale.x, s.scale.y, s.scale.z};
    if (classify_sprite(classifier, view) == 1)
        return false;
    space(p, 4);
    header(p, s, 4);
    constexpr std::array<float, 4> xs{-50, 50, 50, -50}, ys{50, 50, -50, -50};
    constexpr std::array<std::uint32_t, 4> us{0, 0x3f7f7cee, 0x3f7f7cee, 0}, vs{0, 0, 0x3f7f7cee, 0x3f7f7cee};
    const auto &m = view.camera.camera_to_world_rotation;
    for (unsigned j = 0; j < 4; ++j)
    {
        const double x = double(xs[j]) * s.scale.x, y = double(s.scale.y) * ys[j], z = 0.0 * double(s.scale.z);
        const float tx = static_cast<float>((x * m[0] + z * m[8]) + y * m[4]);
        const float ty = static_cast<float>((x * m[1] + z * m[9]) + y * m[5]);
        const double tz = (z * m[10] + y * m[6]) + x * m[2];
        attribute(p, us[j], vs[j], s.vertices[j].color);
        auto &dst = p.positions[p.position_count++];
        dst.x = static_cast<float>(double(tx) + s.position.x);
        dst.y = static_cast<float>(double(ty) + s.position.y);
        dst.z = static_cast<float>(tz + s.position.z);
    }
    return true;
}
} // namespace sgn::retail
