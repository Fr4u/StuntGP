#include "sgn/source_clip_initialization.hpp"
#include "sgn/source_clip_vertex_alias.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
void initialize_source_clip_storage_427270(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x427270, "clip initialization world expired");
    platform->require_owner_thread();
    (void)publish_source_clip_vertex_alias_427270(platform); // 427270..42727A
    auto &storage = platform->state().polygon_frame.clip;
    reset_world_polygon_clip_storage(storage); // 42727F CALL427240
    for (std::uint32_t i = 0; i < 16; ++i)
        storage.sequential_indices[i] = static_cast<std::uint16_t>(i); // 427284..427297
} // 427299 RET
} // namespace sgn::retail
