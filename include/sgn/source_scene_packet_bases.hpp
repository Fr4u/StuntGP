#pragma once
#include <cstdint>
#include <memory>
namespace sgn::retail
{
class RaceSessionPlatform;
struct WorldPolygonFrameStoreObserver;
struct SourceScenePacketBasesResult
{
    std::uint32_t next_required_instruction{0x42e572};
};
// Exactly42E55A/560/566/56C after the separately callable77 keyboard prefix.
// Only packet bases2,3,0,1. No current/group/total/payload/lease resets.
SourceScenePacketBasesResult reset_source_scene_packet_bases_42e55a(const std::weak_ptr<RaceSessionPlatform> &,
                                                                    WorldPolygonFrameStoreObserver * = nullptr);
} // namespace sgn::retail
