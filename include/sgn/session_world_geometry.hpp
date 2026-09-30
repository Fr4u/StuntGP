#pragma once
#include "sgn/retail_track_geometry_animation.hpp"
#include "sgn/retail_world_polygons.hpp"

namespace sgn::diagnostics
{
class SessionWorldJoin;
}
namespace sgn::retail
{
class RaceSession;
// Optional diagnostic storage only. These borrows do not select geometry,
// rewrite Session owners or supply a replacement producer.
struct SessionWorldGeometryAudit
{
    TrackGeometryAnimationAudit track;
    WorldPolygonClipAudit *clipping{};
    diagnostics::SessionWorldJoin *world_join{}; // Optional host diagnostic; OFF unless explicitly supplied.
};
// Exact42E870 tail,0042E89D..0042E8C1:42E800,42B040,4277D0,429A40,stamp.
// The caller has already completed the viewport/camera/light/entity prefix.
// On a failure after the first source write, retain performed writes, publish
// the lighting cursor and invalidate the level geometry lease. The caller
// must abort that level; do not retry or reattach its partially advanced state.
void finish_session_world_geometry(RaceSession &, SessionWorldGeometryAudit = {});
} // namespace sgn::retail
