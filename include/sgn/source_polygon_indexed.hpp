#pragma once
#include <memory>
#include <cstdint>

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourcePolygonIndexedPrefixResult
{
    static constexpr std::uint32_t next_instruction = 0x42e48c;
};
SourcePolygonIndexedPrefixResult allocate_source_polygon_indexed_42e47b(const std::weak_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
