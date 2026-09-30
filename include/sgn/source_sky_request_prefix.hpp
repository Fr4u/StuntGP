#pragma once
#include "sgn/source_pmd_requests.hpp"

namespace sgn::retail
{
class RaceSessionPlatform;
// Bounded42C740 extension: the actual request producer followed, on its sky
// branch only, by the completed425880 clear. Stops before42C7B7 reads58C808.
// NEW admission: one live source platform, creating thread, already prepared
// canonical scene. Caller retains request initialization and all host setup.
// No externally supplied boundary or owner pair can authorize a resume.
SourceRequestBoundary produce_source_requests_through_sky_clear_42c7b7(RaceSessionPlatform &, LevelBlock,
                                                                       SourceRequestObserver * = nullptr);
} // namespace sgn::retail
