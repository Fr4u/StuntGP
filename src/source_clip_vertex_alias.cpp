#include "sgn/source_clip_vertex_alias.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceClipVertexAliasResult publish_source_clip_vertex_alias_427270(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x427270, "clip alias world expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x427270, "clip alias requires source world");
    auto &frame = platform->state().polygon_frame;
    const auto base = frame.transformed.allocation; // 427270 read61C50C
    // Checked native representation of guest base+60000; no payload read/copy.
    frame.clip.vertices = base.source_subspan(0x60000 / 32, 0x18000 / 32); // 427275,42727A ->6244D4
    return {}; // 42727F CALL427240 still required; no cursor/index-table reset here
}
} // namespace sgn::retail
