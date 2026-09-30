#pragma once
#include "sgn/race_session.hpp"
#include "sgn/retail_camera_frame.hpp"

namespace sgn::retail
{
// Build at the call site; every persistent field remains owned by RaceSession.
inline CameraFrameWorld camera_frame_world(RaceSessionState &s, const SplineSet &splines,
                                           const contact::TrackGeometry &track, ModelResources *models = nullptr)
{
    return {s.vehicles,
            s.controls,
            s.camera_lifecycle,
            s.camera_presentation,
            s.camera_bank,
            s.model_draws.template_pose,
            s.camera_session,
            splines,
            track,
            s.gravity,
            s.clock,
            s.ranking,
            s.current_slot,
            s.selected_camera_preset,
            s.split_setting,
            s.overlays,
            s.contact_workspace,
            s.contact_cast,
            s.lights,
            s.light_selection,
            s.light_fade,
            s.light_animation,
            s.light_vertices,
            s.light_vertex_offset,
            s.frame_projection,
            s.inverse_depth_scale,
            s.depth_offset,
            s.viewport,
            s.shared_model_identity,
            s.view_matrices,
            models};
}
inline CameraFrameWorld camera_frame_world(RaceSession &session)
{
    return camera_frame_world(session.state(), session.splines(), session.track_geometry(), &session.models());
}
} // namespace sgn::retail
