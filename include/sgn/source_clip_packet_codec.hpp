#pragma once
#include "sgn/world_polygon_packet_binding.hpp"
namespace sgn::retail
{
// Scope-bound adapter: all reads/stores use the actual five-DWORD packet.
// No cached host WorldPolygonPacket or duplicated payload. Callers keep this
// object alive while using spans/references returned by its synchronous reads.
class SourceClipPacketCodec
{
    std::shared_ptr<RaceSessionPlatform> platform_;
    WorldPolygonPacketPointer packet_;
    SourceAllocationBank &bank() const;
    std::span<std::uint8_t> pointed_token(std::uint32_t, std::size_t stride, std::size_t alignment) const;
    std::span<std::uint8_t> pointed(unsigned lane, std::size_t stride, std::size_t alignment) const;
    std::uint32_t encode(const SourceClipCurrentPointer &) const;

  public:
    SourceClipPacketCodec(const std::weak_ptr<RaceSessionPlatform> &, WorldPolygonPacketPointer);
    SourceClipCurrentPointer publish_output_4276c8(); // four stores, retaining mask DWORD+10
    // 427543 captures only the source pointer word. It is dereferenced only
    // by a reached427576, after the header count has taken the nonzero branch.
    std::uint32_t capture_indices_427543() const;
    std::uint16_t index_at_427576(std::uint32_t captured_token, std::uint32_t ordinal) const;
    WorldPolygonHeaderRef header() const;
    std::span<const std::uint16_t> indices() const;
    WorldPolygonAttributeView attributes() const;
    std::span<WorldPolygonVertex> vertices() const;
    std::uint32_t mask() const;
    void reject_427520() const;
};
} // namespace sgn::retail
