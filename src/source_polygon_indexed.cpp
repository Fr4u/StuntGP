#include "sgn/source_polygon_indexed.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourcePolygonIndexedPrefixResult allocate_source_polygon_indexed_42e47b(
    const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x42e47b, "indexed allocation world expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x42e47b, "indexed source allocation requires source world");
    auto &frame = platform->state().polygon_frame;
    if (frame.allocations)
        throw SourceAllocationError(0x42e47b, "indexed source allocation cannot coexist with preview payloads");
    if (!frame.transformed.allocation.source_bound())
        throw SourceAllocationError(0x42e47b, "indexed source binding requires prior vertex allocation publication");
    const auto allocation = platform->source_allocations().allocate_42e8d0(0xc000, 0); // 42E47B
    auto words = SourceWorldSpan<std::uint32_t>::source(platform, allocation, 0xc000);
    frame.indexed.publish_base(
        WorldIndexedVertexPointer::source(std::move(words), frame.transformed.allocation)); // 42E487,61C5EC
    // No61C520 write. The current pointer may still name a previous allocation;
    // 42E800/42ACE7, not this allocation, copies the new base into it.
    return {}; // 42E48C begins packet allocations; eight allocations still remain
}
} // namespace sgn::retail
