#include "sgn/retail_world_model_lighting.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace sgn::retail
{
namespace
{
float f32(double value) noexcept
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
std::uint32_t load(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}
void store(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
std::uint32_t ftol(double value) noexcept
{
    // 462860 truncates to signed64 and returns its low32. Masked invalid FISTP
    // writes8000000000000000, whose low32 are zero. No out-of-range C++ cast.
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(value)));
}
void validate_lights(const WorldModelLightingPose &pose, const PmdLightBank &lights,
                     const LightViewSelection &selection, const LightVertexCursor &prepared)
{
    if (prepared.buffer.size() != 0x4800 || selection.active_count > 16 ||
        selection.active_count > selection.ordered.size())
        throw FormatError("source model lighting workspace/list bounds");
    if (pose.part_count > 1 && !pose.inverse_part)
        throw FormatError("source model lighting part matrix is absent");
    for (std::uint32_t i = 0; i < selection.active_count; ++i)
        if (!selection.ordered[i] || *selection.ordered[i] >= lights.records.size())
            throw FormatError("source model light identity is absent");
}
void validate_coordinates(const WorldModelLightingPart &part, std::uint32_t stamp, std::uint32_t stride)
{
    if (stride == 0)
        throw FormatError("source model lighting zero stride would not terminate");
    for (std::uint64_t i = (stride - 1) & stamp; i < part.coordinates.size(); i += stride)
    {
        const auto index = part.coordinates[static_cast<std::size_t>(i)][2];
        if ((index & 0xffffu) >= part.vertices.size() || (index >> 16) >= part.normals.size())
            throw FormatError("source model lighting coordinate vertex/normal index");
    }
}
std::array<std::uint32_t, 3> ambient_color(const RetailAmbientLight &ambient, std::uint16_t multiplier) noexcept
{
    auto result = ambient.packed;
    if (multiplier > 1)
        for (auto &value : result)
            value = (value * std::uint32_t(multiplier)) >> 8;
    return result;
}
float dot(std::span<const std::uint8_t> prepared, std::size_t offset, const VehicleVector &normal)
{
    return f32((double(as_float(load(prepared, offset + 8))) * normal.z +
                double(as_float(load(prepared, offset + 4))) * normal.y) +
               double(as_float(load(prepared, offset))) * normal.x);
}
void shade(const WorldModelLightingPose &pose, WorldModelLightingPart part, PmdLightBank &lights,
           const LightViewSelection &selection, LightVertexCursor &prepared, const RetailAmbientLight &ambient,
           const ColorTables *tables, std::uint32_t stamp, std::uint32_t stride)
{
    // New host preflight validates borrowed extents before source writes.
    validate_lights(pose, lights, selection, prepared);
    validate_coordinates(part, stamp, stride);
    const auto before = tables ? std::array<std::uint32_t, 3>{} : ambient_color(ambient, part.model_ambience);
    prepare_world_model_lights(pose, lights, selection, prepared);
    const auto base = tables ? ambient_color(ambient, part.model_ambience) : before;
    for (std::uint64_t i = (stride - 1) & stamp; i < part.coordinates.size(); i += stride)
    {
        auto &coordinate = part.coordinates[static_cast<std::size_t>(i)];
        const auto &vertex = part.vertices[coordinate[2] & 0xffffu];
        const auto &normal = part.normals[coordinate[2] >> 16];
        auto color = base;
        std::uint32_t specular = 0;
        for (std::uint32_t j = 0; j < selection.active_count; ++j)
        {
            const auto offset = std::size_t(j) * 0x24;
            const auto identity = load(prepared.buffer, offset + 0x20);
            if (identity == 0 || identity > lights.records.size())
                throw FormatError("source prepared light token");
            const auto &light = lights.records[identity - 1];
            if (light.kind > 1)
                continue;
            const float incidence = dot(prepared.buffer, offset, normal);
            // Source tests the stored float's sign bit, including negative zero.
            if ((as_word(incidence) & 0x80000000u) == 0)
                continue;
            if (light.kind == 0)
            {
                const auto intensity = ftol(incidence);
                for (unsigned c = 0; c < 3; ++c)
                    color[c] -= light.scaled_color[c] * intensity;
                if (tables)
                {
                    const float highlight = dot(prepared.buffer, offset + 0x10, normal);
                    if ((as_word(highlight) & 0x80000000u) != 0)
                        specular -= ftol(highlight);
                }
            }
            else
            {
                const double dx = double(light.relative.x) - vertex.x, dy = double(light.relative.y) - vertex.y,
                             dz = double(light.relative.z) - vertex.z;
                const double distance = (dz * dz + dy * dy) + dx * dx;
                const auto intensity = ftol((double(light.strength) * 4177920.0) / distance);
                for (unsigned c = 0; c < 3; ++c)
                    color[c] += light.scaled_color[c] * intensity;
            }
        }
        for (auto &component : color)
            component = std::min(component, 0xff00u);
        if (!tables)
            coordinate[3] = (((color[2] & 0xff00u) | 0x10000u) << 8) | (color[1] & 0xff00u) | (color[0] >> 8);
        else
        {
            const auto shifted = std::bit_cast<std::int32_t>(specular) >> (8 + (part.part_flags_02 & 3));
            const auto index = static_cast<unsigned>(std::clamp(shifted, 0, 255));
            const auto gamma = tables->raster_gamma[index];
            coordinate[3] =
                ((((gamma & 0xffffff00u) << 8) | (color[2] & 0xff00u)) << 8) | (color[0] >> 8) | (color[1] & 0xff00u);
        }
    }
}
} // namespace
void prepare_world_model_lights(const WorldModelLightingPose &pose, PmdLightBank &lights,
                                const LightViewSelection &selection, LightVertexCursor &prepared)
{
    validate_lights(pose, lights, selection, prepared);
    const std::array<float, 3> camera{f32(double(pose.normalized_camera.x) * 255.0),
                                      f32(double(pose.normalized_camera.y) * 255.0),
                                      f32(double(pose.normalized_camera.z) * 255.0)};
    CameraMatrix scratch{};
    const CameraMatrix *rotation = &pose.inverse_outer;
    if (pose.part_count > 1)
    {
        vehicle_multiply_basis(scratch, pose.inverse_outer, *pose.inverse_part);
        rotation = &scratch;
    }
    const Vec3 position{f32(double(pose.part_position.x) + pose.outer_position.x),
                        f32(double(pose.part_position.y) + pose.outer_position.y),
                        f32(double(pose.part_position.z) + pose.outer_position.z)};
    prepared.offset = 0;
    for (std::uint32_t i = 0; i < selection.active_count; ++i)
    {
        const auto index = *selection.ordered[i];
        auto &light = lights.records[index];
        const auto offset = prepared.offset;
        const double x = double(as_float(light.opaque_30_4c[0])) * 255.0,
                     y = double(as_float(light.opaque_30_4c[1])) * 255.0,
                     z = double(as_float(light.opaque_30_4c[2])) * 255.0;
        std::array<float, 3> direction;
        for (unsigned c = 0; c < 3; ++c)
            direction[c] = f32((x * (*rotation)[c] + z * (*rotation)[c + 8]) + y * (*rotation)[c + 4]);
        prepared.offset += 0x24;
        for (unsigned c = 0; c < 3; ++c)
            store(prepared.buffer, offset + c * 4, as_word(direction[c]));
        light.relative.x = f32(double(light.position.x) - position.x);
        light.relative.y = f32(double(light.position.y) - position.y);
        light.relative.z = f32(double(light.position.z) - position.z);
        const double sx = double(camera[0]) + direction[0];
        const float sy = f32(double(camera[1]) + direction[1]), sz = f32(double(camera[2]) + direction[2]);
        store(prepared.buffer, offset + 0x10, as_word(f32((sx * 765.0) * 0.5)));
        store(prepared.buffer, offset + 0x14, as_word(f32(double(f32(double(sy) * 765.0)) * 0.5)));
        store(prepared.buffer, offset + 0x18, as_word(f32(double(f32(double(sz) * 765.0)) * 0.5)));
        store(prepared.buffer, offset + 0x20, index + 1);
    }
}
void shade_world_model_diffuse(const WorldModelLightingPose &pose, WorldModelLightingPart part, PmdLightBank &lights,
                               const LightViewSelection &selected, LightVertexCursor &prepared,
                               const RetailAmbientLight &ambient, std::uint32_t stamp, std::uint32_t stride)
{
    shade(pose, part, lights, selected, prepared, ambient, nullptr, stamp, stride);
}
void shade_world_model_specular(const WorldModelLightingPose &pose, WorldModelLightingPart part, PmdLightBank &lights,
                                const LightViewSelection &selected, LightVertexCursor &prepared,
                                const RetailAmbientLight &ambient, const ColorTables &tables, std::uint32_t stamp,
                                std::uint32_t stride)
{
    shade(pose, part, lights, selected, prepared, ambient, &tables, stamp, stride);
}
void select_world_model_lighting_stride(std::uint32_t &stride, std::uint32_t quality, std::uint32_t current,
                                        std::uint32_t previous) noexcept
{
    stride = quality || current != previous ? 1u : 4u;
}
void select_world_part_lighting_stride(std::uint32_t &stride, std::uint32_t quality, std::uint32_t current,
                                       std::uint32_t previous, std::uint16_t part_word) noexcept
{
    stride = quality || current != previous || (part_word >= 2 && part_word <= 5) ? 1u : 4u;
}
} // namespace sgn::retail
