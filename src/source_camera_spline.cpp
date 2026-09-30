#include "sgn/source_camera_spline.hpp"
#include "sgn/race_session.hpp"
#include <cstring>
#include <bit>
#include <limits>
namespace sgn::retail
{
std::uint32_t SourceCameraSplineRef::read_word(std::uint64_t relative, std::uint32_t site) const
{
    const auto platform = platform_.lock();
    if (!platform)
        throw SourceAllocationError(site, "camera spline platform expired");
    platform->require_owner_thread();
    try
    {
        const auto bytes = platform->source_allocations().bytes(allocation_);
        // Offsets are normalized host metadata, not x86 addresses. Wrapping
        // would forge a different location in this same allocation.
        if (relative > std::uint64_t(std::numeric_limits<std::uint32_t>::max()) - offset_)
            throw SourceAllocationError(site, "camera spline normalized offset exceeds source32-bit domain");
        const auto at = std::uint64_t(offset_) + relative;
        if (at > bytes.size() || bytes.size() - at < 4)
            throw SourceAllocationError(site, "camera spline DWORD outside live allocation");
        std::uint32_t word;
        std::memcpy(&word, bytes.data() + at, 4);
        if constexpr (std::endian::native == std::endian::big)
            word = std::byteswap(word);
        return word;
    }
    catch (const SourceAllocationError &error)
    {
        throw SourceAllocationError(site, error.what());
    }
}
std::optional<SourceCameraSplineRef> find_source_camera_spline_428800(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                                                      std::uint32_t id)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x428801, "camera lookup platform expired");
    platform->require_owner_thread();
    const auto &track = platform->source_scene_resources().models().source_loading_state().track;
    const auto count = track.spline_count; // 428801 captures624498, unsigned comparison
    if (count == 0)
        return {};                       // 42880C: do not read624474 or touch its allocation
    const auto location = track.splines; // 42880E captured actual pointer
    SourceCameraSplineRef base(borrow, location.allocation, location.offset);
    std::uint64_t relative = 0;
    for (std::uint32_t index = 0; index < count; ++index, relative += 28u)
    {
        // The reached id read checks the widened stride/base sum. No eager
        // count-wide extent or descriptor/payload validation is introduced.
        if (base.read_word(relative, 0x428816) == id)
        {
            const auto offset = std::uint32_t(std::uint64_t(location.offset) + relative);
            return SourceCameraSplineRef(borrow, location.allocation, offset); // other24 bytes untouched
        }
    }
    return {};
}
} // namespace sgn::retail
