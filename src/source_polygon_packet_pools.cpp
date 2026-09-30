#include "sgn/source_polygon_packet_pools.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourcePolygonPacketPoolsPrefixResult allocate_source_polygon_packet_pools_42e48c(
    const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x42e48c, "packet allocation world expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x42e48c, "packet source allocation requires source world");
    auto &frame = platform->state().polygon_frame;
    if (frame.allocations)
        throw SourceAllocationError(0x42e48c, "source packet buffers cannot coexist with preview payloads");
    auto &bank = platform->source_allocations();
    using Record = std::array<std::uint32_t, 5>;
    static_assert(sizeof(Record) == 20);
    const auto inside_opaque = bank.allocate_42e8d0(0xf000, 0); // 42E48C
    frame.packets[2].publish_base(WorldPolygonPacketPointer::source(
        SourceWorldSpan<Record>::source(platform, inside_opaque, 0xf000))); // 42E498,61C5F4
    const auto inside_alpha = bank.allocate_42e8d0(0xf000, 0);              // 42E49D
    frame.packets[3].publish_base(WorldPolygonPacketPointer::source(
        SourceWorldSpan<Record>::source(platform, inside_alpha, 0xf000))); // 42E4A9,61C5F8
    const auto clipped_opaque = bank.allocate_42e8d0(0xf000, 0);           // 42E4AE
    frame.packets[0].publish_base(WorldPolygonPacketPointer::source(
        SourceWorldSpan<Record>::source(platform, clipped_opaque, 0xf000))); // 42E4BA,61C5FC
    const auto clipped_alpha = bank.allocate_42e8d0(0xf000, 0);              // 42E4BF
    frame.packets[1].publish_base(WorldPolygonPacketPointer::source(
        SourceWorldSpan<Record>::source(platform, clipped_alpha, 0xf000))); // 42E4CB,61C600
    // Each base publication is independent. No current pointer, group range,
    // total, indexed cursor, vertex count or lease is changed. No rollback.
    return {}; // four remaining allocations start at42E4D0; not full42E430
}
} // namespace sgn::retail
