#include "sgn/source_scene_packet_bases.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceScenePacketBasesResult reset_source_scene_packet_bases_42e55a(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                                                    WorldPolygonFrameStoreObserver *observer)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw FormatError("packet base reset platform expired");
    platform->require_owner_thread();
    auto &frame = platform->state().polygon_frame;
    constexpr std::array<unsigned, 4> pc{0x42e566, 0x42e56c, 0x42e55a, 0x42e560};
    constexpr std::array<unsigned, 4> address{0x61c5fc, 0x61c600, 0x61c5f4, 0x61c5f8};
    for (const auto p : {2u, 3u, 0u, 1u})
    {
        frame.packets[p].clear_base();
        if (observer)
            observer->stored(pc[p], address[p], frame);
    }
    return {};
}
} // namespace sgn::retail
