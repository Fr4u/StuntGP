#include "sgn/retail_vehicle_anchors.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
void bounds(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t size)
{
    require(offset <= bytes.size() && size <= bytes.size() - offset, "anchor PMD read outside file");
}
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    bounds(bytes, offset, 4);
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}
std::uint16_t half(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    bounds(bytes, offset, 2);
    std::uint16_t value;
    std::memcpy(&value, bytes.data() + offset, 2);
    return value;
}
const VehicleAnchorModel &model(const VehicleAnchorVehicle &v)
{
    require(v.model != nullptr, "active vehicle anchor has no model binding");
    return *v.model;
}
} // namespace
VehicleAnchorModelView::VehicleAnchorModelView(std::span<const std::uint8_t> bytes, unsigned selected_lod)
    : bytes_(bytes), selected_lod_(selected_lod)
{
    bounds(bytes, 0, 0x148);
    require(std::string_view(reinterpret_cast<const char *>(bytes.data()), 10) == std::string_view("PMD V1.83\0", 10),
            "unsupported anchor PMD header");
    auto section = [&](unsigned offset, unsigned size, unsigned stride, std::size_t &start, std::size_t &length) {
        start = 0x148 + std::size_t(word(bytes, offset));
        length = word(bytes, size);
        bounds(bytes, start, length);
        require(length % stride == 0, "misaligned anchor PMD section");
    };
    section(0x48, 0xdc, 64, parts_, part_bytes_);
    section(0x44, 0xd8, 80, transforms_, transform_bytes_);
    section(0x2c, 0xc0, 16, vertices_, vertex_bytes_);
    const std::array<std::pair<std::size_t, std::size_t>, 3> ranges{
        {{parts_, part_bytes_}, {transforms_, transform_bytes_}, {vertices_, vertex_bytes_}}};
    for (unsigned i = 0; i < ranges.size(); ++i)
        for (unsigned j = i + 1; j < ranges.size(); ++j)
        {
            auto [a, n] = ranges[i];
            auto [b, m] = ranges[j];
            require(!n || !m || a + n <= b || b + m <= a, "overlapping anchor PMD sections");
        }
    descriptor_ = 0x148 + std::size_t(word(bytes, 0x4c));
    bounds(bytes, descriptor_, 0x68);
    part_count_ = word(bytes, descriptor_ + 0x24);
    auto last_lod = word(bytes, descriptor_ + 0x58);
    require(part_count_ > 0 && last_lod < 4 && selected_lod <= last_lod, "invalid anchor PMD LOD/count");
    lod_count_ = last_lod + 1;
    for (unsigned i = 0; i < lod_count_; ++i)
    {
        first_parts_[i] = word(bytes, descriptor_ + 0x48 + i * 4);
        require(std::uint64_t(first_parts_[i]) + part_count_ <= part_bytes_ / 64,
                "anchor PMD part range outside section");
        for (unsigned j = 0; j < i; ++j)
            require(std::uint64_t(first_parts_[i]) + part_count_ <= first_parts_[j] ||
                        std::uint64_t(first_parts_[j]) + part_count_ <= first_parts_[i],
                    "overlapping relocated anchor LOD parts");
    }
}
std::size_t VehicleAnchorModelView::part_offset(unsigned index, unsigned lod) const
{
    require(lod < lod_count_, "invalid anchor LOD index");
    const auto absolute = std::uint64_t(first_parts_[lod]) + index;
    require(absolute < part_bytes_ / 64, "anchor part outside PMD section");
    // Source boundary predicates can reach the following LOD. That access is
    // portable only if42ECA0 relocated the reached record as an authored part.
    bool relocated = false;
    for (unsigned i = 0; i < lod_count_; ++i)
        relocated |= absolute >= first_parts_[i] && absolute - first_parts_[i] < part_count_;
    require(relocated, "anchor access reaches an unrelocated PMD part");
    return parts_ + std::size_t(absolute) * 64;
}
VehicleVector VehicleAnchorModelView::vector(std::size_t at) const
{
    return {std::bit_cast<float>(word(bytes_, at)), std::bit_cast<float>(word(bytes_, at + 4)),
            std::bit_cast<float>(word(bytes_, at + 8)), std::bit_cast<float>(word(bytes_, at + 12))};
}
VehicleAnchorPart VehicleAnchorModelView::part(unsigned index, unsigned lod) const
{
    const auto at = part_offset(index, lod);
    const auto transform = word(bytes_, at + 8);
    require(transform < transform_bytes_ / 80, "anchor transform outside PMD section");
    const auto tr = transforms_ + std::size_t(transform) * 80;
    return {half(bytes_, at), vector(tr), vector(tr + 16), vector(tr + 32)};
}
std::uint16_t VehicleAnchorModelView::part_flags(unsigned index, unsigned lod) const
{
    return half(bytes_, part_offset(index, lod));
}
float VehicleAnchorModelView::maximum_vertex_z(unsigned index, unsigned lod) const
{
    const auto at = part_offset(index, lod);
    const auto first = word(bytes_, at + 0x30);
    const auto count = half(bytes_, at + 0x28);
    require(std::uint64_t(first) + count <= vertex_bytes_ / 16, "anchor vertices outside PMD section");
    float maximum = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        auto value = std::bit_cast<float>(word(bytes_, vertices_ + (std::size_t(first) + i) * 16 + 8));
        require(std::isfinite(value), "nonfinite anchor vertex");
        if (maximum < value)
            maximum = value;
    }
    return maximum;
}
void initialize_vehicle_extent_anchors(VehicleAnchorWorld world)
{
    for (auto &v : world.vehicles)
    {
        if (v.race.active != 1)
            continue;
        const auto &m = model(v);
        if (m.part_count() <= 11)
        {
            v.presentation.field_3134 = 0;
            continue;
        }
        auto maximum = m.maximum_vertex_z(11, m.selected_lod());
        auto part = m.part(11, m.selected_lod());
        v.anchors.forward = part.forward;
        v.anchors.forward.x = std::bit_cast<float>(std::bit_cast<std::uint32_t>(part.forward.x) ^ 0x80000000u);
        v.anchors.position = part.position;
        v.anchors.extent = maximum * std::bit_cast<float>(0x3f7d70a4u);
        v.presentation.field_3134 = 1; // 46e3b8
    }
    world.current_slot = 0;
}
void initialize_vehicle_effect_anchors(VehicleAnchorWorld world)
{
    for (auto &v : world.vehicles)
    {
        if (v.race.active != 1)
            continue;
        v.anchors.effect_available = {0, 0};
        const auto &m = model(v);
        if (m.part_count() <= 19)
            continue;
        v.anchors.effect_available[0] = 1;
        auto first = m.part(19, m.selected_lod());
        v.anchors.effect_position[0] = first.position;
        v.anchors.effect_base_scale[0] = first.scale;
        // 416350 reads part20's flag before the part-count comparison.
        if (m.part_flags(20, m.selected_lod()) != 1 || m.part_count() <= 20)
            continue;
        auto second = m.part(20, m.selected_lod());
        v.anchors.effect_available[1] = 1;
        v.anchors.effect_position[1] = second.position;
        v.anchors.effect_base_scale[1] = second.scale;
    }
    world.current_slot = 0;
}
void initialize_vehicle_pair_a_anchors(VehicleAnchorWorld world)
{
    for (auto &v : world.vehicles)
    {
        if (v.race.active != 1)
            continue;
        const auto &m = model(v);
        if (m.part_count() < 23)
            continue;
        v.trails.pair_a[0] = m.part(23, 0).position;
        v.trails.pair_a[1] = m.part(24, 0).position;
    }
    world.current_slot = 0;
}
void initialize_vehicle_pair_b_anchors(VehicleAnchorWorld world)
{
    for (auto &v : world.vehicles)
    {
        v.trails.pair_b_available = {0, 0};
        if (v.race.active != 1)
            continue;
        const auto &m = model(v);
        if (m.part_count() < 21)
            continue;
        v.trails.pair_b_available[0] = 1;
        v.trails.pair_b[0] = m.part(21, 0).position;
        if (m.part_count() < 22)
            continue;
        v.trails.pair_b_available[1] = 1;
        v.trails.pair_b[1] = m.part(22, 0).position;
    }
    world.current_slot = 0;
}
} // namespace sgn::retail
