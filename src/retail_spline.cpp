#include "sgn/retail_spline.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <limits>
#include <string_view>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t at)
{
    require(at <= data.size() && data.size() - at >= 4, "spline read outside supplied PMD");
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) | (std::uint32_t(data[at + 2]) << 16) |
           (std::uint32_t(data[at + 3]) << 24);
}
float real(std::span<const std::uint8_t> data, std::size_t at)
{
    return std::bit_cast<float>(word(data, at));
}
bool finite(Vec3 p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
Vec3 xyz(VehicleVector v)
{
    return {v.x, v.y, v.z};
}
VehicleVector difference(VehicleVector a, VehicleVector b)
{
    return {vehicle_f32(double(a.x) - b.x), vehicle_f32(double(a.y) - b.y), vehicle_f32(double(a.z) - b.z), 0};
}
void valid(SplineView s)
{
    require(s.descriptor.point_count > 0 && s.descriptor.point_count <= 0x7fffffffu,
            "spline count outside supported signed range");
    require(std::isfinite(s.descriptor.length) && s.descriptor.length > 0, "invalid spline length");
}
std::uint32_t retail_index(std::int32_t index, std::uint32_t count)
{
    const auto bits = std::bit_cast<std::uint32_t>(index);
    return index < 0 ? count - ((0u - bits) % count) : bits % count;
}
std::int32_t add_index(std::int32_t value, std::uint32_t delta)
{
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) + delta);
}
} // namespace
SplineSet::SplineSet(Bytes pmd) : storage_(std::move(pmd))
{
    const auto bytes = std::span<const std::uint8_t>(storage_);
    require(bytes.size() >= 0x148, "truncated PMD spline header");
    constexpr std::string_view signature = "PMD V1.83";
    require(std::equal(signature.begin(), signature.end(), bytes.begin()) && bytes[signature.size()] == 0,
            "unsupported spline PMD version");
    const std::uint64_t point_at = 0x148ull + word(bytes, 0x74), desc_at = 0x148ull + word(bytes, 0x78);
    const auto point_bytes = word(bytes, 0x108), desc_bytes = word(bytes, 0x10c);
    require(point_bytes % 16 == 0 && desc_bytes % 28 == 0, "misaligned PMD spline sections");
    require(point_at + point_bytes <= bytes.size() && desc_at + desc_bytes <= bytes.size(),
            "PMD spline section outside file");
    point_storage_end_ = static_cast<std::size_t>(point_at + point_bytes);
    require(!point_bytes || !desc_bytes || point_at + point_bytes <= desc_at || desc_at + desc_bytes <= point_at,
            "overlapping PMD spline sections");
    for (std::uint32_t i = 0; i < desc_bytes / 28; ++i)
    {
        const auto at = static_cast<std::size_t>(desc_at + std::uint64_t(i) * 28);
        SplineDescriptor d{word(bytes, at),      word(bytes, at + 4),
                           word(bytes, at + 8),  {word(bytes, at + 12), word(bytes, at + 16), word(bytes, at + 20)},
                           real(bytes, at + 24), 0};
        require((std::uint64_t(d.point_index) + d.point_count) * 16 <= point_bytes,
                "spline points outside point section");
        d.point_offset = static_cast<std::size_t>(point_at + std::uint64_t(d.point_index) * 16);
        valid({d, bytes});
        for (std::uint32_t p = 0; p < d.point_count; ++p)
        {
            const auto v = spline_point({d, bytes, point_storage_end_}, p).value;
            require(finite(xyz(v)), "non-finite PMD spline point");
        }
        descriptors_.push_back(d);
    }
}
std::optional<SplineView> SplineSet::find(std::uint32_t id) const
{
    for (const auto &d : descriptors_)
        if (d.id == id)
            return SplineView{d, storage_, point_storage_end_};
    return std::nullopt;
}
SplinePoint spline_point(SplineView spline, std::uint32_t index)
{
    const auto at64 = std::uint64_t(spline.descriptor.point_offset) + std::uint64_t(index) * 16;
    const auto end = spline.point_storage_end ? spline.point_storage_end : spline.storage.size();
    require(end <= spline.storage.size() && at64 <= end && end - at64 >= 16,
            "original spline point access exceeds authored point storage");
    const auto at = static_cast<std::size_t>(at64);
    VehicleVector v{real(spline.storage, at), real(spline.storage, at + 4), real(spline.storage, at + 8),
                    real(spline.storage, at + 12)};
    require(finite(xyz(v)), "non-finite spline point access");
    return {v, index >= spline.descriptor.point_count};
}
IndexedSplineSample sample_spline_indexed(SplineView spline, float distance)
{
    valid(spline);
    require(std::isfinite(distance), "non-finite spline distance");
    const auto count = spline.descriptor.point_count;
    const double scaled = (double(count) * distance) / spline.descriptor.length;
    require(std::isfinite(scaled) && scaled >= -2147483648.0 && scaled < 2147483648.0,
            "spline sample index outside signed range");
    const auto index = retail_index(static_cast<std::int32_t>(scaled), count);
    const auto first = spline_point(spline, index), next = spline_point(spline, (index + 1u) % count);
    const auto delta = difference(next.value, first.value);
    const double fraction = std::fmod(double(vehicle_f32(scaled)), 1.0);
    const float x = vehicle_f32(fraction * delta.x), y = vehicle_f32(fraction * delta.y);
    VehicleVector result{vehicle_f32(double(x) + first.value.x), vehicle_f32(double(y) + first.value.y),
                         vehicle_f32(fraction * delta.z + first.value.z), first.value.w};
    require(finite(xyz(result)), "spline interpolation overflow");
    return {result, index, first.adjacent_storage_read || next.adjacent_storage_read};
}
SplineSample sample_spline(SplineView spline, float distance)
{
    const auto result = sample_spline_indexed(spline, distance);
    return {result.value, result.adjacent_storage_read};
}
SplineProjection project_spline(SplineView spline, Vec3 position, std::int32_t segment)
{
    valid(spline);
    require(finite(position), "non-finite spline projection position");
    const auto count = spline.descriptor.point_count;
    int direction = 0;
    bool adjacent = false;
    for (std::uint64_t examined = 1; examined <= std::uint64_t(count) * 2 + 2; ++examined)
    {
        const auto index = retail_index(segment, count);
        const auto first = spline_point(spline, index), next = spline_point(spline, (index + 1u) % count);
        adjacent |= first.adjacent_storage_read || next.adjacent_storage_read;
        float length{};
        const auto delta = vehicle_normalize(difference(next.value, first.value), &length);
        require(std::isfinite(length) && length > 0, "degenerate projection segment");
        const double projection =
            ((double(position.z) - first.value.z) * delta.z + (double(position.y) - first.value.y) * delta.y) +
            (double(position.x) - first.value.x) * delta.x;
        const float stored = vehicle_f32(projection);
        if (projection < 0 && direction <= 0)
        {
            segment = add_index(segment, 0xffffffffu);
            direction = -1;
            continue;
        }
        if (projection >= 0 && stored >= length && direction >= 0)
        {
            segment = add_index(segment, 1);
            direction = 1;
            continue;
        }
        const float percent = vehicle_f32(((double(stored) / length + segment) * 100.0) / count);
        require(std::isfinite(percent), "spline projection overflow");
        return {segment, percent, static_cast<std::uint32_t>(examined), adjacent};
    }
    throw FormatError("original spline projection failed to converge within two traversals");
}
StartingGrid starting_grid(SplineView spline, const std::array<std::int32_t, 6> &active, std::int32_t global_mode)
{
    valid(spline);
    require(spline.descriptor.id == 1, "original starting grid requires spline ID 1");
    const auto first = spline_point(spline, 0).value,
               last = spline_point(spline, spline.descriptor.point_count - 1).value;
    auto forward = vehicle_normalize(difference(first, last));
    VehicleVector up{0, 1, 0, 0};
    require(forward.x != 0 || forward.y != 0 || forward.z != 0, "degenerate starting grid direction");
    // Authored vectors, EXE 004743f0..0047444f; no runtime grid design parameters.
    constexpr std::array<VehicleVector, 6> offsets = {{{500, 0, -2000, 0},
                                                       {-500, 0, -3000, 0},
                                                       {500, 0, -4000, 0},
                                                       {-500, 0, -5000, 0},
                                                       {500, 0, -6000, 0},
                                                       {-500, 0, -7000, 0}}};
    StartingGrid result{};
    for (std::uint32_t i = 0; i < 6; ++i)
        if (active[i] == 1)
        {
            auto right = vehicle_normalize(vehicle_cross(forward, up));
            up = vehicle_normalize(up);
            forward = vehicle_normalize(forward);
            require(right.x != 0 || right.y != 0 || right.z != 0, "degenerate starting grid basis");
            Vec3 position = xyz(first);
            std::optional<float> w;
            if (global_mode == 3)
                w = first.w;
            else
            {
                const auto p = offsets[i];
                const float x = vehicle_f32((double(p.y) * up.x + double(p.z) * forward.x) + double(p.x) * right.x);
                const float y = vehicle_f32((double(p.y) * up.y + double(p.z) * forward.y) + double(right.y) * p.x);
                const float z = vehicle_f32((double(p.y) * up.z + double(p.z) * forward.z) + double(right.z) * p.x);
                position = {vehicle_f32(double(x) + first.x), vehicle_f32(double(y) + first.y),
                            vehicle_f32(double(z) + first.z)};
            }
            require(finite(position), "starting grid position overflow");
            result[i] = GridPose{position, {xyz(right), xyz(up), xyz(forward)}, i, w};
        }
    return result;
}
DistanceStartPlacement prepare_distance_start(SplineView spline, float distance)
{
    const auto sample = sample_spline_indexed(spline, distance);
    const auto first = spline_point(spline, sample.selected_index);
    const auto next = spline_point(spline, (sample.selected_index + 1u) % spline.descriptor.point_count);
    const auto heading = vehicle_normalize(difference(next.value, first.value));
    const std::uint32_t progress = (sample.selected_index * 100u) / spline.descriptor.point_count;
    return {sample.value,
            xyz(heading),
            sample.selected_index,
            progress,
            progress - 1u,
            vehicle_f32(double(std::bit_cast<std::int32_t>(progress))),
            {sample.selected_index, sample.selected_index, sample.selected_index},
            sample.adjacent_storage_read || first.adjacent_storage_read || next.adjacent_storage_read};
}
RespawnPlacement select_respawn(SplineView spline, std::int32_t requested, std::span<const float, 128> mask)
{
    valid(spline);
    const auto count = spline.descriptor.point_count;
    auto candidate = requested;
    for (std::uint64_t advanced = 0; advanced <= count; ++advanced)
    {
        const auto index = retail_index(candidate, count);
        const auto mask_index = ((index << 7) - index) / count;
        require(mask_index < mask.size(), "respawn mask index outside array");
        require(std::isfinite(mask[mask_index]), "non-finite respawn mask value");
        if (mask[mask_index] == 1.0f)
        {
            const auto first = spline_point(spline, index), next = spline_point(spline, index + 1u);
            const auto heading = vehicle_normalize(difference(next.value, first.value));
            const auto persistent = add_index(requested, static_cast<std::uint32_t>(advanced));
            return {first.value,
                    xyz(heading),
                    persistent,
                    {persistent, persistent, persistent},
                    index,
                    static_cast<std::uint32_t>(advanced),
                    first.adjacent_storage_read || next.adjacent_storage_read};
        }
        candidate = std::bit_cast<std::int32_t>(index + 1u);
    }
    throw FormatError("respawn mask has no eligible point within one traversal");
}
std::array<SplineView, 3> ai_spline_routes(const SplineSet &splines)
{
    std::array<SplineView, 3> result{};
    for (std::uint32_t i = 0; i < 3; ++i)
    {
        auto route = splines.find(i + 3);
        if (!route)
            route = splines.find(1);
        require(route.has_value(), "AI route and fallback spline ID 1 are missing");
        result[i] = *route;
    }
    return result;
}
RespawnMask decode_respawn_mask(std::span<const std::uint8_t> bytes)
{
    const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    require(text.find('\0') == std::string_view::npos, "embedded NUL in level configuration");
    const auto key = text.find("-CarReplacementSplineMask[]");
    require(key != std::string_view::npos, "level has no replacement spline mask");
    auto at = text.find('=', key);
    require(at != std::string_view::npos, "replacement mask has no equals sign");
    at = text.find(' ', at);
    require(at != std::string_view::npos, "replacement mask has no value separator");
    RespawnMask result{};
    for (auto &value : result)
    {
        while (at < text.size() && (text[at] == ' ' || text[at] == '\r' || text[at] == '\n'))
            ++at;
        const auto end = text.find(' ', at);
        require(end != std::string_view::npos && end > at, "replacement mask has fewer than 128 delimited values");
        const auto first = at + (text[at] == '+' ? 1 : 0), number_end = end - (text[end - 1] == ',' ? 1 : 0);
        double parsed{};
        const auto conversion =
            std::from_chars(text.data() + first, text.data() + number_end, parsed, std::chars_format::general);
        require(conversion.ec == std::errc{} && conversion.ptr == text.data() + number_end && std::isfinite(parsed),
                "invalid replacement mask number");
        value = vehicle_f32(parsed);
        require(std::isfinite(value), "replacement mask number outside binary32 range");
        at = end;
    }
    return result;
}
} // namespace sgn::retail
