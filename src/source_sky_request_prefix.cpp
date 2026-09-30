#include "sgn/source_sky_request_prefix.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/retail_world_view_prefix.hpp"

namespace sgn::retail
{
SourceRequestBoundary produce_source_requests_through_sky_clear_42c7b7(RaceSessionPlatform &platform, LevelBlock block,
                                                                       SourceRequestObserver *observer)
{
    // Host admission precedes every source producer store. None of these
    // accesses constructs assets, scene, PMD, requests, audio or a new world.
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw FormatError("sky request prefix requires the source construction entry");
    auto &models = platform.source_scene_resources().models();
    auto &sky = platform.state().texture_animation.sky;
    auto result = models.produce_source_requests_42c740_prefix(block, observer);
    // The result comes only from the above real call on this platform's owner.
    // A throwing observer, including after42C7AB, never reaches this clear.
    if (result.next_site == 0x42c7b2u && result.required_callee == 0x425880u && !result.reached_return)
    {
        clear_sky_texture_motion(sky); // completed425880; no per-store interruption contract
        result.next_site = 0x42c7b7u;
        result.required_callee = 0; // next instruction is a read, not a call
    }
    return result;
}
} // namespace sgn::retail
