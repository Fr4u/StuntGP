#pragma once
#include <memory>
#include <cstdint>
namespace sgn::retail
{
class RaceSessionPlatform;
struct SourceSpriteAllocationsResult
{
    static constexpr std::uint32_t next_instruction = 0x4340f9;
};
// Six real42E8D0 calls/publications only.4340F9 and434030 are NOT performed.
SourceSpriteAllocationsResult allocate_source_sprite_buffers_434090(const std::weak_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
