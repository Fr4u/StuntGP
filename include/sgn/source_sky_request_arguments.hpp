#pragma once
#include "sgn/source_sky_request_count.hpp"

namespace sgn::retail
{
// Typed source address VALUE from PUSH4777FC, not a host pointer or a ready
// literal reader. No bytes at this address are dereferenced by this prefix.
enum class SourceSkyNeedleAddress : std::uint32_t
{
    anim_sky_ground = 0x4777fc
};
struct SourceSkyRequestCallArguments
{
    SourceRequestName haystack;     // first argument: captured existing weak alias
    SourceSkyNeedleAddress needle;  // second argument: original address value
    std::uint32_t edx_count_58c808; // the earlier captured EDX, never a reread
};
struct SourceSkyRequestCallBoundary
{
    SourceRequestBoundary boundary;
    std::optional<SourceSkyRequestCallArguments> arguments;
};
// One synchronous actual producer -> clear -> count -> pointer-value prefix.
// Stops BEFORE CALL462890 at42C7CA. No supplied boundary/resume capability.
// Owner/script lifetime and non-reentrant read-only observer contract are
// inherited from the actual count prefix. No name/literal character reads,
// substring result, source-call execution or later animation stores.
SourceSkyRequestCallBoundary produce_source_requests_through_sky_call_42c7ca(RaceSessionPlatform &, LevelBlock,
                                                                             SourceRequestObserver * = nullptr);
} // namespace sgn::retail
