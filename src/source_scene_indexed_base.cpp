#include "sgn/source_scene_indexed_base.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceSceneIndexedBaseResult reset_source_scene_indexed_base_42e53c(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                                                    SourceSceneIndexedBaseObserver *observer)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw FormatError("indexed base reset platform expired");
    platform->require_owner_thread();
    auto &frame = platform->state().polygon_frame;
    frame.indexed.clear_base(); // 42E53C:61C5EC=0,61C520 and all payloads survive
    if (observer)
        observer->cleared(0x42e53c, 0x61c5ec, frame);
    return {};
}
} // namespace sgn::retail
