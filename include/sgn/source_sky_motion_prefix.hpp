#pragma once
#include "sgn/source_sky_request_arguments.hpp"
#include "sgn/source_request_search.hpp"

namespace sgn::retail
{
struct SourceSkyMotionStore
{
    std::uint32_t site, target, bits, bytes, ordinal;
    bool operator==(const SourceSkyMotionStore &) const = default;
};
struct SourceSkyMotionStoreObserver
{
    virtual ~SourceSkyMotionStoreObserver() = default;
    // NEW read-only observation after one reached DWORD store in the SAME
    // platform world. Throwing keeps exactly that completed source prefix.
    virtual void after_store(const SourceSkyMotionStore &, const RaceSessionPlatform &) = 0;
};
enum class SourceTrackKeyAddress : std::uint32_t
{
    l_track = 0x47780c
};
struct SourceTrackSearchValues
{
    std::uint32_t ecx_script_cursor; // 42C7FE value from existing local EBP-4 slot
    SourceTrackKeyAddress key;       // 42C801 PUSH address VALUE only
    // 42C806 EDX refers to that same local cursor slot. No dangling host
    // pointer is exported: these are diagnostic values, never a resume token.
};
struct SourceSkyMotionBoundary
{
    SourceRequestBoundary boundary;
    std::optional<SourceRequestName> search_result; // engaged-null means callee returned null
    std::optional<SourceTrackSearchValues> track_search_values;
};
// One actual producer ->clear ->count ->arguments ->462890 ->conditional
// four-store prefix. Stops before42C809 CALL42C580 on the sky branch.
// Existing no-sky results are forwarded unchanged, not rerun or rewound.
// Same source platform, requests and bound application literal owner only.
// All callbacks are synchronous, read-only, non-reentrant; they may throw.
// No externally supplied boundary/result/owner pair can authorize a resume.
SourceSkyMotionBoundary produce_source_requests_through_sky_motion_42c809(
    RaceSessionPlatform &, LevelBlock, SourceRequestObserver *producer_observer = nullptr,
    SourceSearchReadObserver *search_observer = nullptr, SourceSkyMotionStoreObserver *store_observer = nullptr);
} // namespace sgn::retail
