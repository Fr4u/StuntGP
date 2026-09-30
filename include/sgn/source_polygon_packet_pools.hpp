#pragma once
#include <memory>
#include <cstdint>

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourcePolygonPacketPoolsPrefixResult
{
    static constexpr std::uint32_t next_instruction = 0x42e4d0;
};
SourcePolygonPacketPoolsPrefixResult allocate_source_polygon_packet_pools_42e48c(
    const std::weak_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
