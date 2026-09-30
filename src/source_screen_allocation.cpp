#include "sgn/source_screen_base.hpp"
#include "sgn/race_session.hpp"
#include <cstddef>

namespace sgn::retail
{
SourceScreenBasePublicationResult allocate_source_screen_base_42e4d0(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                                                     SourceScreenBaseStoreObserver *observer)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x42e4d0, "screen allocation world expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x42e4d0, "screen allocation requires source world");
    static_assert(sizeof(SourceScreenVertex) == 0x20 && sizeof(SourceScreenPolygon) == 0x110);
    static_assert(offsetof(SourceScreenPolygon, vertices) == 0x10);
    auto &state = platform->state();
    const auto allocation = platform->source_allocations().allocate_42e8d0(0x11000, 0); // 42E4D0
    state.screen_polygons.bind_source(SourceWorldSpan<SourceScreenPolygon>::source(platform, allocation, 0x11000));
    state.screen_base.owner_begin_ = static_cast<bool>(allocation); // 42E4DC ->61C5F0, including NULL
    if (observer)
        observer->stored(0x42e4dc, 0x61c5f0, state);
    return {}; // next42E4E1; no count/cursor reset, rollback or source-frame admission
}
} // namespace sgn::retail
