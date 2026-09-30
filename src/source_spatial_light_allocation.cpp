#include "sgn/source_spatial_light_allocation.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
void allocate_source_spatial_light_tail_42e4e1(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x42e4e1, "spatial/light allocation world expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x42e4e1, "spatial/light requires source world");
    auto &state = platform->state();
    auto &frame = state.polygon_frame;
    if (frame.allocations)
        throw SourceAllocationError(0x42e4e1, "source spatial buffers cannot coexist with preview payloads");
    auto &bank = platform->source_allocations();
    using Raw = std::array<std::uint32_t, 2>;
    static_assert(sizeof(Raw) == 8);
    const auto clipped = bank.allocate_42e8d0(0x1000, 0); // 42E4E1
    frame.spatial[0].records =
        WorldSpatialRecords::source(SourceWorldSpan<Raw>::source(platform, clipped, 0x1000)); // 42E4ED,61C518
    const auto inside = bank.allocate_42e8d0(0x1000, 0);                                      // 42E4F2
    frame.spatial[1].records =
        WorldSpatialRecords::source(SourceWorldSpan<Raw>::source(platform, inside, 0x1000));          // 42E4FE,61C51C
    const auto light = bank.allocate_42e8d0(0x4800, 0);                                               // 42E503
    state.light_vertices.bind_source(SourceWorldSpan<std::uint8_t>::source(platform, light, 0x4800)); // 42E508,624530
    // 42E50D RET. Existing spatial counts and light cursor624574 unchanged.
    // No rollback, cursor reset, source-frame admission or fabricated spatial codec.
}
} // namespace sgn::retail
