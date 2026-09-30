#pragma once
#include "sgn/race_session.hpp"
#include "sgn/retail_world_view_prefix.hpp"

namespace sgn::retail
{
// Invocation-only references. The loaded PMD bank and all mutable words stay
// in RaceSession; failure to load the required bank is never an empty fallback.
inline WorldViewPrefix world_view_prefix(RaceSessionState &s)
{
    if (!s.texture_animations)
        throw FormatError("world preparation requires loaded track texture animation");
    return {s.view_quality,      s.impact_particles.rate_selector,
            s.view_matrices,     *s.texture_animations,
            s.texture_animation, s.frame_pause};
}
inline WorldViewPrefix world_view_prefix(RaceSession &session)
{
    return world_view_prefix(session.state());
}
} // namespace sgn::retail
