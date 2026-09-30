#include "sgn/source_sky_request_count.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
SourceSkyRequestCountBoundary produce_source_requests_through_sky_count_42c7bd(RaceSessionPlatform &platform,
                                                                               LevelBlock block,
                                                                               SourceRequestObserver *observer)
{
    auto boundary = produce_source_requests_through_sky_clear_42c7b7(platform, block, observer);
    if (boundary.next_site != 0x42c7b7u || boundary.required_callee != 0 || boundary.reached_return)
        return {boundary, std::nullopt};
    // No callback or persistent source store intervenes after the clear.
    // Accessors borrow the already admitted owner and never construct one.
    const auto &requests = platform.source_scene_resources().models().source_loading_state().requests;
    const auto captured_count = requests.read_count_58c808_42c7b7();
    boundary.next_site = 0x42c7bdu;
    return {boundary, captured_count};
}
} // namespace sgn::retail
