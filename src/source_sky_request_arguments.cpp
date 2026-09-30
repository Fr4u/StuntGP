#include "sgn/source_sky_request_arguments.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
SourceSkyRequestCallBoundary produce_source_requests_through_sky_call_42c7ca(RaceSessionPlatform &platform,
                                                                             LevelBlock block,
                                                                             SourceRequestObserver *observer)
{
    auto prefix = produce_source_requests_through_sky_count_42c7bd(platform, block, observer);
    if (prefix.boundary.next_site != 0x42c7bdu || prefix.boundary.required_callee != 0 ||
        prefix.boundary.reached_return)
        return {prefix.boundary, std::nullopt};
    // NEW internal contract check; the pinned child always carries EDX here.
    if (!prefix.edx_count_58c808)
        throw SourceRequestError(0x42c7c2, "actual count prefix omitted captured EDX");
    const auto needle = SourceSkyNeedleAddress::anim_sky_ground; // 42C7BD immediate, no character read
    const auto &requests = platform.source_scene_resources().models().source_loading_state().requests;
    const auto haystack = requests.read_record_name_42c7c2(*prefix.edx_count_58c808);          // 42C7C2
    const SourceSkyRequestCallArguments arguments{haystack, needle, *prefix.edx_count_58c808}; // 42C7C9 value
    prefix.boundary.next_site = 0x42c7cau;
    prefix.boundary.required_callee = 0x462890u;
    return {prefix.boundary, arguments};
}
} // namespace sgn::retail
