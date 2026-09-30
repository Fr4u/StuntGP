#pragma once
#include "sgn/session_world_materials.hpp"
#include "sgn/polygon_submission_capacity.hpp"

namespace sgn::diagnostics
{
class SessionWorldJoin;
}
namespace sgn::retail
{
class RaceSession;
struct SessionPolygonCapabilities
{
    const std::uint32_t &depth;        // selected device descriptor+64
    const std::uint32_t &multitexture; // the same592074 owner used by42FA80
};
// Synchronous host binding for44260B -> full433F40. Geometry has already been
// produced, clipped and projected, and61C508 has already advanced. This call
// neither produces geometry nor changes a clock/camera/RNG. Materials and
// scratch must be the persistent owners also used by source loading callbacks.
// Caller must retain source capabilities and material-name owners throughout
// the call. Deferred sinks must own all draw payloads before returning.
// Candidate: requires receiving Session/whole-submission qualification.
void submit_session_world_polygons(RaceSession &, const SessionWorldMaterials &, SessionPolygonCapabilities,
                                   PolygonSubmissionState &, PolygonSubmissionSink &, PolygonSubmissionBudget,
                                   diagnostics::SessionWorldJoin *world_join = nullptr);
} // namespace sgn::retail
