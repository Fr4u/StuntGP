#include "sgn/source_clip_packet_codec.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceClipPacketCodec::SourceClipPacketCodec(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                             WorldPolygonPacketPointer packet)
    : platform_(borrow.lock()), packet_(std::move(packet))
{
    if (!platform_)
        throw SourceAllocationError(0x4276c8, "packet platform expired");
    platform_->require_owner_thread();
    if (!packet_.source_bound())
        throw SourceAllocationError(0x4276c8, "packet codec requires raw source records");
    packet_.require_source_platform(platform_);
}
SourceAllocationBank &SourceClipPacketCodec::bank() const
{
    platform_->require_owner_thread();
    return platform_->source_allocations();
}
std::uint32_t SourceClipPacketCodec::encode(const SourceClipCurrentPointer &cursor) const
{
    const auto &location = cursor.source_pointer();
    if (location.platform.owner_before(platform_) || platform_.owner_before(location.platform) ||
        location.bank != &bank())
        throw SourceAllocationError(0x4276c8, "clip pointer belongs to another platform");
    return bank().encode_address(location.allocation, location.byte_offset);
}
SourceClipCurrentPointer SourceClipPacketCodec::publish_output_4276c8()
{
    (void)bank(); // owner thread before any world access
    auto &clip = platform_->state().polygon_frame.clip;
    const auto attributes = clip.attribute_count;     // 4276C8
    const auto header = clip.header_count;            // 4276CE
    packet_.store_source_word(2, encode(attributes)); // 4276D4
    const auto vertices = clip.vertex_count;          // 4276D7
    packet_.store_source_word(0, encode(header));     // 4276DD
    packet_.store_source_word(1, 0x6244a8);           // 4276DF: same world's16WORD owner
    packet_.store_source_word(3, encode(vertices));   // 4276E6
    return header;                                    // captured ECX, not a later re-read of6244DC or the packet
}
std::span<std::uint8_t> SourceClipPacketCodec::pointed_token(std::uint32_t token, std::size_t stride,
                                                             std::size_t alignment) const
{
    const auto location = bank().decode_address(token);
    if (!location.allocation)
        throw SourceAllocationError(0x427530, "packet pointer is NULL");
    auto bytes = bank().known_prefix(location.allocation, location.offset, 0x427530);
    if (reinterpret_cast<std::uintptr_t>(bytes.data()) % alignment != 0)
        throw SourceAllocationError(0x427530, "packet pointer alignment outside typed domain");
    return bytes.first(bytes.size() / stride * stride);
}
std::span<std::uint8_t> SourceClipPacketCodec::pointed(unsigned lane, std::size_t stride, std::size_t alignment) const
{
    return pointed_token(packet_.source_word(lane), stride, alignment);
}
std::uint32_t SourceClipPacketCodec::capture_indices_427543() const
{
    (void)bank();
    return packet_.source_word(1);
}
std::uint16_t SourceClipPacketCodec::index_at_427576(std::uint32_t token, std::uint32_t ordinal) const
{
    if (token == 0x6244a8)
    {
        (void)bank();
        const auto &indices = platform_->state().polygon_frame.clip.sequential_indices;
        if (ordinal >= indices.size())
            throw SourceAllocationError(0x427576, "source sequential index outside16-word owner");
        return indices[ordinal];
    }
    const auto location = bank().decode_address(token);
    const auto at = std::uint64_t(location.offset) + std::uint64_t(ordinal) * 2u;
    if (at > 0xffffffffu)
        throw SourceAllocationError(0x427576, "source index offset outside DWORD domain");
    if (at % alignof(std::uint16_t))
        throw SourceAllocationError(0x427576, "captured source index alignment outside typed domain");
    const auto bytes = bank().readable_bytes(location.allocation, static_cast<std::uint32_t>(at), 2, 0x427576);
    std::uint16_t value;
    std::memcpy(&value, bytes.data(), 2);
    return value;
}
WorldPolygonHeaderRef SourceClipPacketCodec::header() const
{
    auto bytes = pointed(0, 8, alignof(WorldPolygonHeader));
    if (bytes.size() < 8)
        throw SourceAllocationError(0x427530, "packet header out of bounds");
    return world_polygon_header_ref(*reinterpret_cast<WorldPolygonHeader *>(bytes.data()));
}
std::span<const std::uint16_t> SourceClipPacketCodec::indices() const
{
    if (packet_.source_word(1) == 0x6244a8)
    {
        platform_->require_owner_thread();
        return platform_->state().polygon_frame.clip.sequential_indices;
    }
    auto bytes = pointed(1, 2, alignof(std::uint16_t));
    return {reinterpret_cast<const std::uint16_t *>(bytes.data()), bytes.size() / 2};
}
WorldPolygonAttributeView SourceClipPacketCodec::attributes() const
{
    auto bytes = pointed(2, 16, alignof(WorldPolygonAttribute));
    return std::span<const WorldPolygonAttribute>{reinterpret_cast<const WorldPolygonAttribute *>(bytes.data()),
                                                  bytes.size() / 16};
}
std::span<WorldPolygonVertex> SourceClipPacketCodec::vertices() const
{
    auto bytes = pointed(3, 32, alignof(WorldPolygonVertex));
    return {reinterpret_cast<WorldPolygonVertex *>(bytes.data()), bytes.size() / 32};
}
std::uint32_t SourceClipPacketCodec::mask() const
{
    (void)bank();
    return packet_.source_word(4);
}
void SourceClipPacketCodec::reject_427520() const
{
    (void)bank();
    packet_.store_source_word(0, 0);
}
} // namespace sgn::retail
