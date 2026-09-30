#pragma once
#include <memory>
#include <cstdint>

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourcePolygonVertexClipPrefixResult
{
    static constexpr std::uint32_t next_instruction = 0x42e47b;
};
// First FOUR real42E8D0 calls and publications of42E430. Nine calls remain.
SourcePolygonVertexClipPrefixResult allocate_source_polygon_vertex_clip_42e430(
    const std::weak_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
