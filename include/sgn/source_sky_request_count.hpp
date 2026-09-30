#pragma once
#include "sgn/source_sky_request_prefix.hpp"
#include <optional>

namespace sgn::retail
{
// Transient result of this synchronous call, not a persistent source owner
// or a capability to resume a different invocation. Empty on the no-sky path.
struct SourceSkyRequestCountBoundary
{
    SourceRequestBoundary boundary;
    std::optional<std::uint32_t> edx_count_58c808;
};
// Actual producer -> completed425880 -> single42C7B7 count read on the same
// platform/scene/request owner. Stops before42C7BD. No supplied boundary,
// request reset, reconstructed owner, name read or later462890 call.
// The platform and borrowed script remain alive; an observer is read-only
// and must not replace/reenter owners. A throwing observer prevents this read.
SourceSkyRequestCountBoundary produce_source_requests_through_sky_count_42c7bd(RaceSessionPlatform &, LevelBlock,
                                                                               SourceRequestObserver * = nullptr);
} // namespace sgn::retail
