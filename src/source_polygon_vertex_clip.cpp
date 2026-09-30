#include "sgn/source_polygon_vertex_clip.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourcePolygonVertexClipPrefixResult allocate_source_polygon_vertex_clip_42e430(
    const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x42e430, "polygon allocation world expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x42e430, "source polygon allocation requires source world entry");
    auto &frame = platform->state().polygon_frame;
    if (frame.allocations)
        throw SourceAllocationError(0x42e430, "source polygon buffers cannot coexist with preview payloads");
    auto &bank = platform->source_allocations();
    const auto vertices = bank.allocate_42e8d0(0x78000, 0); // 42E437
    frame.transformed.allocation =
        SourceWorldSpan<WorldPolygonVertex>::source(platform, vertices, 0x78000); // 42E443,61C50C
    const auto attributes = bank.allocate_42e8d0(0xc000, 0);                      // 42E448
    frame.clip.attributes =
        SourceWorldSpan<WorldPolygonAttribute>::source(platform, attributes, 0xc000);             // 42E454,6244C8
    const auto headers = bank.allocate_42e8d0(0x2000, 0);                                         // 42E459
    frame.clip.headers = SourceWorldSpan<WorldPolygonHeader>::source(platform, headers, 0x2000);  // 42E465,6244CC
    const auto indices = bank.allocate_42e8d0(0x800, 0);                                          // 42E46A
    frame.clip.unused_indices = SourceWorldSpan<std::uint16_t>::source(platform, indices, 0x800); // 42E476,6244D0
    // No reset of transformed.count/clip cursors, clip.vertices6244D4, frame
    // stamp, packet groups, indexed pointers or level lease. No rollback/free:
    // each earlier source allocation/publication survives a later failure.
    return {}; // continue at42E47B, not a claim that full42E430 returned
}
} // namespace sgn::retail
