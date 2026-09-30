#include "sgn/retail_track_normal_allocation.hpp"
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw FormatError("track normal metadata outside PMD");
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}
} // namespace
void bind_track_normal_allocation(std::span<const std::uint8_t> pmd, std::span<VehicleVector> normals,
                                  TrackNormalAllocation &out)
{
    const auto base = std::uint64_t(word(pmd, 0x20)) + 0x148u;
    const auto size = word(pmd, 0xb4), first = word(pmd, 0xe4);
    if (word(pmd, 0x18) != 1 || size % sizeof(VehicleVector) || base > pmd.size() || size > pmd.size() - base ||
        normals.size_bytes() != size || first > normals.size())
        throw FormatError("track normal owner does not match the original PMD section");
    out.first_generated = first; // 42EF80 leaves576CC8 untouched
}
} // namespace sgn::retail
