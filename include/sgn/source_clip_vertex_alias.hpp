#pragma once
#include <memory>
#include <cstdint>
namespace sgn::retail
{
class RaceSessionPlatform;
struct SourceClipVertexAliasResult
{
    static constexpr std::uint32_t next_instruction = 0x42727f;
};
SourceClipVertexAliasResult publish_source_clip_vertex_alias_427270(const std::weak_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
