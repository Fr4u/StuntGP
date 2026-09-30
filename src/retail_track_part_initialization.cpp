#include "sgn/retail_track_part_initialization.hpp"
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t at)
{
    if (at > bytes.size() || bytes.size() - at < 4)
        throw FormatError("track initialization word outside PMD");
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + at, 4);
    return value;
}
} // namespace
void initialize_track_part_resources(std::uint32_t scene, ModelResource *track, std::span<const std::uint8_t> pmd,
                                     std::span<TrackPartAnimation> records, TrackPartPreparationScratch &scratch)
{
    if (scene == 2 || !track)
        return; // 426266..426288
    const auto lod = track->source_lod_index(0);
    const auto count = track->part_count(lod);
    if (count <= 1)
        return;
    if (pmd.size() < 328 || word(pmd, 0x18) != 1 || records.size() < count)
        throw FormatError("track initialization requires its actual track state and PMD");
    const std::size_t descriptor = 328ull + word(pmd, 0x4c), animations = 328ull + word(pmd, 0x60);
    if (word(pmd, descriptor + 0x24) != count || animations > pmd.size() || count > (pmd.size() - animations) / 64)
        throw FormatError("track initialization model and animation table differ");
    for (unsigned i = 0; i < count; ++i)
    {
        const auto at = animations + std::size_t(i) * 64;
        const auto kind = word(pmd, at + 0x30);
        if (kind >= 1 && kind <= 3)
        { // 4262AC..4262C5
            prepare_animated_track_part({track->mutable_source_vertices(lod, i), track->source_coordinates(lod, i),
                                         track->source_attributes(lod, i), track->source_corner_indices(),
                                         track->source_normals(), scratch},
                                        kind);
        }
        auto &record = records[i];
        record.kind = word(pmd, at + 0x30); // source re-read after425DF0
        std::memcpy(&record.angles, pmd.data() + at, 16);
        std::memcpy(&record.angular_velocity, pmd.data() + at + 0x10, 16);
        std::memcpy(&record.parameters, pmd.data() + at + 0x20, 16);
        record.parameter_34 = word(pmd, at + 0x34);
        record.parameter_44 = word(pmd, at + 0x38);
        record.phase_u = 0; // 42634A; phase_v+3C and retained_40 remain untouched
    }
}
} // namespace sgn::retail
