#include "sgn/retail_light_effects.hpp"
#include "sgn/retail_camera_tables.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return vehicle_f32(value);
}
using sgn::bit_reinterpret::as_float;
std::int32_t signed_bits(float value)
{
    return std::bit_cast<std::int32_t>(value);
}
double dot(double x, double y, double z, const CameraVector &normal)
{
    return (z * normal.z + y * normal.y) + x * normal.x;
}
float sine(std::uint32_t index, std::span<const GlareAdjacentSample> adjacent)
{
    if (index < camera_detail::trig_bits.size())
        return as_float(camera_detail::trig_bits[index]);
    for (const auto &sample : adjacent)
        if (sample.index == index)
            return sample.value;
    throw FormatError("glare requires an adjacent live source sine sample");
}
void check_storage(const SourceSpritePacketStorage &pool)
{
    if (pool.source_part.source_bound())
        throw SourceAllocationError(0x434590,
                                    "native sprite-packet view cannot substitute for canonical source cursors");
    if (pool.header_bytes > 4096 || pool.header_bytes % 8 || pool.index_count > 2048 || pool.position_count > 2048 ||
        pool.attribute_count > 2048)
        throw FormatError("sprite packet cursor outside source allocation");
}
} // namespace
GlareState initial_glare_state()
{
    return {{GlareElement{1, 1, 0.5f, 32, 0, 0x804000},
             GlareElement{1, 1, 0.5f, 21, 0x6000, 0x808000},
             {3, 0.5f, as_float(0x3e4ccccd), 32, 0, 0xff},
             {2, 0, as_float(0x3d4ccccd), 32, 0, 0x4c0000},
             {3, -0.5f, as_float(0x3eb33333), 32, 0, 0x800000},
             {2, -1, as_float(0x3d4ccccd), 32, 0, 0x4c0000},
             {3, -1.75f, 0.5f, 32, 0, 0x8000}}};
}
SourceSpritePacketView sprite_packet_view(const SourceSpritePacketStorage &pool)
{
    check_storage(pool);
    return {pool.headers.view().first(pool.header_bytes / 8), pool.indices.view().first(pool.index_count),
            pool.positions.view().first(pool.position_count), pool.attributes.view().first(pool.attribute_count)};
}
std::uint32_t classify_sprite(const SourceSpriteRecord &sprite, LightCameraView view)
{
    const float x = f32(double(sprite.position.x) - view.camera.position.x),
                y = f32(double(sprite.position.y) - view.camera.position.y);
    const double z = double(sprite.position.z) - view.camera.position.z;
    const float depth = f32(dot(x, y, z, view.presentation.forward_axis));
    if (signed_bits(depth) < 0 || signed_bits(f32(double(depth) - view.lifecycle.clip_far)) > 0)
        return 1;
    const float radius_x = f32(double(sprite.scale[0]) * 75), radius_y = f32(double(sprite.scale[1]) * 75);
    std::array<float, 4> planes{};
    for (unsigned i = 0; i < 4; ++i)
    {
        const double distance = dot(x, y, f32(z), view.presentation.planes[i]);
        planes[i] = f32(distance);
        if (signed_bits(f32(distance - (i < 2 ? radius_x : radius_y))) > 0)
            return 1;
    }
    if (signed_bits(f32(double(depth) + double(sprite.scale[2]) * 75)) > 0)
        return 2;
    for (unsigned i = 0; i < 4; ++i)
        if (signed_bits(f32(double(planes[i]) + (i < 2 ? radius_x : radius_y))) > 0)
            return 2;
    return 0;
}
bool append_sprite_packet(SourceSpritePacketStorage &pool, const SourceSpriteRecord &sprite, LightCameraView view)
{
    if (pool.source_part.source_bound())
    {
        return append_source_sprite_packet_434590(pool, sprite, view);
    }
    check_storage(pool);
    if (pool.header_bytes >= 4096)
        return false;
    if (classify_sprite(sprite, view) == 1)
        return false;
    if (pool.index_count > 2044 || pool.position_count > 2044 || pool.attribute_count > 2044)
        throw FormatError("sprite packet stream exceeds source allocation");
    ++pool.polygons;
    pool.vertices = static_cast<std::uint16_t>(pool.vertices + 4);
    pool.corners = static_cast<std::uint16_t>(pool.corners + 4);
    auto &header = pool.headers[pool.header_bytes / 8];
    header[0] = 4;
    header[1] = static_cast<std::uint8_t>(sprite.texture);
    header[2] = static_cast<std::uint8_t>(sprite.alpha);
    header[3] = static_cast<std::uint8_t>(sprite.flags | 0x80);
    header[6] = static_cast<std::uint8_t>(pool.index_count);
    header[7] = static_cast<std::uint8_t>(pool.index_count >> 8);
    pool.header_bytes += 8;
    pool.up = sprite.up;
    const auto &up = pool.up;
    const auto &forward = pool.forward;
    pool.right.x = f32(double(forward.z) * up.y - double(forward.y) * up.z);
    pool.right.y = f32(double(forward.x) * up.z - double(up.x) * forward.z);
    pool.right.z = f32(double(up.x) * forward.y - double(forward.x) * up.y);
    constexpr std::array<float, 4> xs{-50, 50, 50, -50}, ys{50, 50, -50, -50};
    constexpr std::array<std::uint32_t, 4> us{0, 0x3f7f7cee, 0x3f7f7cee, 0}, vs{0, 0, 0x3f7f7cee, 0x3f7f7cee};
    const auto &matrix = view.camera.camera_to_world_rotation;
    for (unsigned i = 0; i < 4; ++i)
    {
        const double a = double(xs[i]) * sprite.scale[0], b = double(sprite.scale[1]) * ys[i],
                     c = 0.0 * double(sprite.scale[2]);
        auto &attribute = pool.attributes[pool.attribute_count];
        attribute.vertex = static_cast<std::uint16_t>(pool.position_count);
        attribute.color = sprite.colors[0];
        attribute.u = us[i];
        attribute.v = vs[i];
        const float x = f32((double(forward.x) * c + double(up.x) * b) + double(pool.right.x) * a);
        const float y = f32((double(forward.y) * c + double(pool.right.y) * a) + double(up.y) * b);
        const double z = (double(pool.right.z) * a + double(forward.z) * c) + double(up.z) * b;
        const float world_x = f32((z * matrix[8] + double(y) * matrix[4]) + double(x) * matrix[0]);
        const float world_y = f32((double(x) * matrix[1] + z * matrix[9]) + double(y) * matrix[5]);
        const double world_z = (double(x) * matrix[2] + z * matrix[10]) + double(y) * matrix[6];
        auto &position = pool.positions[pool.position_count++];
        position.x = f32(double(world_x) + sprite.position.x);
        position.y = f32(double(world_y) + sprite.position.y);
        position.z = f32(world_z + sprite.position.z);
        pool.indices[pool.index_count++] = static_cast<std::uint16_t>(pool.attribute_count++);
    }
    return true;
}
void prepare_animated_light_view(PmdLightBank &bank, LightViewSelection &selected, LightFadeState &fade,
                                 LightViewAnimation &animation, LightCameraView view, LightVertexCursor &vertex_cursor,
                                 std::uint32_t split, LightGammaDevice *gamma, CameraMatrix *rotation_audit)
{
    if (vertex_cursor.buffer.size() != 0x4800 || vertex_cursor.offset > vertex_cursor.buffer.size())
        throw FormatError("invalid light vertex workspace");
    prepare_light_view(bank, selected, fade, view, split, gamma);
    if (selected.active_count > 16)
        throw FormatError("active lights exceed original 16-pointer list");
    animation.phase_fast = (animation.phase_fast + 32) & 8191;
    animation.phase_slow = (animation.phase_slow + 2) & 8191;
    vertex_cursor.offset = 0;
    for (std::uint32_t i = 0; i < selected.active_count; ++i)
    {
        auto &light = bank.records.at(selected.ordered.at(i).value());
        const auto matrix = camera_rotation(std::bit_cast<std::int32_t>(light.opaque_54_64[0]),
                                            std::bit_cast<std::int32_t>(light.opaque_54_64[1]),
                                            std::bit_cast<std::int32_t>(light.opaque_54_64[2]));
        if (rotation_audit)
            *rotation_audit = matrix;
        for (unsigned j = 0; j < 3; ++j)
            light.opaque_30_4c[j] = std::bit_cast<std::uint32_t>(
                f32((double(matrix[8 + j]) * 1 + double(matrix[4 + j]) * 0) + double(matrix[j]) * 0));
    }
}
void append_light_glare(const RetailLightRecord &light, LightFadeState &fade, const LightViewAnimation &animation,
                        GlareState &glare, SourceSpritePacketStorage &packets, LightCameraView view,
                        std::span<const GlareAdjacentSample> adjacent)
{
    // Validate host-only borrowed sample availability before any source mutation.
    for (unsigned i = 0; i < 2; ++i)
        (void)sine(glare.elements[i].phase, adjacent);
    if (animation.phase_slow >= 8192)
        throw FormatError("glare slow phase outside source cycle");
    const auto &camera = view.camera;
    const auto &axis = view.presentation.forward_axis;
    const double near_scaled = double(view.lifecycle.clip_near) * as_float(0x3f99999a);
    const float near = f32(near_scaled);
    const CameraVector center{f32(near_scaled * axis.x + camera.position.x),
                              f32(double(f32(double(near) * axis.y)) + camera.position.y),
                              f32(double(f32(double(near) * axis.z)) + camera.position.z), 0};
    CameraVector direction{f32(double(light.position.x) - camera.position.x),
                           f32(double(light.position.y) - camera.position.y),
                           f32(double(light.position.z) - camera.position.z), 0};
    const double length =
        (double(direction.x) * direction.x + double(direction.y) * direction.y) + double(direction.z) * direction.z;
    if (length == 0 || std::isnan(length))
        direction.x = direction.y = direction.z = 0;
    else
    {
        const double inv = 1.0 / std::sqrt(length);
        direction.x = f32(inv * direction.x);
        direction.y = f32(inv * direction.y);
        direction.z = f32(inv * direction.z);
    }
    const double alignment = dot(direction.x, direction.y, direction.z, axis);
    const float stored_alignment = f32(alignment);
    const double ratio = double(near) / alignment;
    const float point_x = f32(double(f32(ratio * direction.x)) + camera.position.x),
                point_y = f32(double(f32(ratio * direction.y)) + camera.position.y);
    const CameraVector delta{f32(double(point_x) - center.x), f32(double(point_y) - center.y),
                             f32((ratio * direction.z + camera.position.z) - center.z), 0};
    if (alignment >= as_float(0x3f5db3d7))
    {
        double value = (alignment - as_float(0x3f5db3d7)) * as_float(0x40eed9ea);
        value *= value;
        value *= value;
        fade.target = f32(value * 5 + 1);
    }
    else
        fade.target = 1;
    double strength = double(light.strength) * (light.kind == 0 ? 0.25 : double(as_float(0x3ccccccd)));
    if (strength > 0.25)
        strength = 0.25;
    const double square = double(stored_alignment) * stored_alignment;
    const float base_scale = f32(((square * square) * strength) * 2.5);
    SourceSpriteRecord sprite;
    sprite.flags = 0x58;
    sprite.texture = 1;
    sprite.alpha = 0x80;
    sprite.colors.fill(0xffffff);
    sprite.scale = {1, 1, 1};
    sprite.opaque_34_44 = {0, 0, 0, 0xbf800000, 0};
    sprite.up = {0, 1, 0, 0};
    sprite.opaque_58_64 = {1, 0, 0, 2};
    for (unsigned i = 0; i < 7; ++i)
    {
        auto &element = glare.elements[i];
        double scale = double(base_scale) * element.scale;
        sprite.position.x = f32(double(element.displacement) * delta.x + center.x);
        sprite.position.y = f32(double(f32(double(element.displacement) * delta.y)) + center.y);
        sprite.position.z = f32(double(f32(double(element.displacement) * delta.z)) + center.z);
        if (i < 2)
        {
            const double x = double(light.position.x) - camera.position.x,
                         y = double(light.position.y) - camera.position.y,
                         z = double(light.position.z) - camera.position.z;
            double distance = (std::sqrt((z * z + y * y) + x * x) * as_float(0x358637bd)) * 2000;
            if (!(distance >= 1))
                distance = 1;
            scale = distance + double(sine(element.phase, adjacent)) * 4;
            element.phase = (element.step + element.phase) & 8191;
            sprite.position = {light.position.x, light.position.y, light.position.z, as_float(light.position.opaque)};
            const auto phase = i == 1 ? (0u - animation.phase_slow) & 8191 : animation.phase_slow;
            sprite.up.x = as_float(camera_detail::trig_bits[phase + 2048]);
            sprite.up.y = as_float(camera_detail::trig_bits[phase]);
            sprite.up.z = 0;
        }
        else if (i == 2)
        {
            sprite.up.x = 0;
            sprite.up.y = 1;
            sprite.up.z = 0;
        }
        sprite.texture = static_cast<std::uint16_t>(element.texture);
        sprite.colors.fill(element.color | 0xff000000u);
        sprite.scale.fill(f32(scale));
        append_sprite_packet(packets, sprite, view);
    }
}
} // namespace sgn::retail
