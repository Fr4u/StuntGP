#include "sgn/track_part_animation.hpp"
#include "track_phase_record.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint32_t word(std::span<const std::uint8_t> b, std::size_t at)
{
    if (at > b.size() || b.size() - at < 4)
        throw FormatError("track part animation field outside PMD");
    std::uint32_t result;
    std::memcpy(&result, b.data() + at, 4);
    return result;
}
struct Section
{
    std::size_t offset;
    std::uint32_t count;
};
Section section(std::span<const std::uint8_t> b)
{
    if (b.size() < 328 || std::string_view(reinterpret_cast<const char *>(b.data()), 9) != "PMD V1.83" ||
        word(b, 0x18) != 1)
        throw FormatError("track part animation requires actual track PMD");
    const std::size_t descriptor = 328ull + word(b, 0x4c), table = 328ull + word(b, 0x60);
    const auto count = word(b, descriptor + 0x24);
    // Bounded modern loader, not a claim of an original allocation limit.
    if (count > 32 || table > b.size() || std::uint64_t(count) * 64 > b.size() - table)
        throw FormatError("track animation table outside supported source bank");
    return {table, count};
}
void rotate(TrackPartAnimation &p)
{ // 427050: source keeps X in x87 across its store
    constexpr float step = std::bit_cast<float>(0x3c888889u);
    const double dx = double(p.angular_velocity.x) * step;
    const float dy = vehicle_f32(double(p.angular_velocity.y) * step),
                dz = vehicle_f32(double(p.angular_velocity.z) * step);
    const double x = dx + p.angles.x;
    p.angles.x = vehicle_f32(x);
    const float y = vehicle_f32(double(dy) + p.angles.y), z = vehicle_f32(double(dz) + p.angles.z);
    p.angles.y = y;
    p.angles.z = z;
    if (!(x >= 0))
        p.angles.x = vehicle_f32(x + 360);
    else if (x > 360)
        p.angles.x = vehicle_f32(x - 360);
    if (!(y >= 0))
        p.angles.y = vehicle_f32(double(y) + 360);
    else if (y > 360)
        p.angles.y = vehicle_f32(double(y) - 360);
    if (!(z >= 0))
        p.angles.z = vehicle_f32(double(z) + 360);
    else if (z > 360)
        p.angles.z = vehicle_f32(double(z) - 360);
}
} // namespace
void load_track_part_animation_records(std::span<TrackPartAnimation> records, std::span<const std::uint8_t> pmd)
{
    const auto table = section(pmd);
    if (table.count <= 1)
        return;
    if (records.size() < table.count)
        throw FormatError("track animation destination too short");
    for (unsigned i = 0; i < table.count; ++i)
    {
        auto &record = records[i];
        const auto at = table.offset + std::size_t(i) * 64;
        record.kind = word(pmd, at + 0x30);
        std::memcpy(&record.angles, pmd.data() + at, 16);
        std::memcpy(&record.angular_velocity, pmd.data() + at + 0x10, 16);
        std::memcpy(&record.parameters, pmd.data() + at + 0x20, 16);
        record.parameter_34 = word(pmd, at + 0x34);
        record.phase_u = 0;
        record.parameter_44 = word(pmd, at + 0x38);
    }
}
TrackPartAnimationBank::TrackPartAnimationBank() : records_(32) {}
TrackPartAnimationBank::TrackPartAnimationBank(std::span<const std::uint8_t> pmd)
{
    records_.resize(section(pmd).count);
    load_track_part_animation_records(records_, pmd);
}
void detail::advance_track_phase_record(TrackPartAnimation &p, std::uint32_t kind)
{
    const auto subtract = [&](std::uint32_t value) { p.phase_u = (p.phase_u - value) & 0x1fffu; };
    switch (kind)
    {
    case 1:
        p.phase_u = (p.phase_u + 0x80u) & 0x1fffu;
        break;
    case 2:
    case 8:
    case 9:
        subtract(0x100);
        break;
    case 3:
        subtract(0xffffff00u);
        break;
    case 4:
        rotate(p);
        break;
    case 5:
    case 6:
    case 10:
        subtract(0x40);
        break;
    case 7:
    case 14:
        subtract(0x20);
        break;
    case 11:
        subtract(0x80);
        break;
    case 12:
        subtract(8);
        break;
    case 13:
        subtract(0x10);
        break;
    case 16:
        rotate(p);
        subtract(0x80);
        p.phase_v = (p.phase_v - 0x2au) & 0x1fffu;
        break;
    default:
        break;
    }
}
void advance_track_part_animations(std::span<TrackPartAnimation> records, std::optional<std::uint32_t> count)
{
    if (!count || *count <= 1)
        return;
    if (*count > records.size())
        throw FormatError("track animation descriptor exceeds owned records");
    for (unsigned i = 1; i < *count; ++i)
    {
        auto &p = records[i];
        detail::advance_track_phase_record(p, p.kind);
    }
}
} // namespace sgn::retail
