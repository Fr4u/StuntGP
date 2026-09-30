#include "sgn/session_world_submission.hpp"
#include "sgn/session_world_join.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void submit_session_world_polygons(RaceSession &session, const SessionWorldMaterials &owner,
                                   SessionPolygonCapabilities capabilities, PolygonSubmissionState &scratch,
                                   PolygonSubmissionSink &sink, PolygonSubmissionBudget budget,
                                   diagnostics::SessionWorldJoin *world_join)
{
    session.shared_platform()->require_owner_thread();
    const auto &materials = owner.validate_for(session);
    auto &state = session.state();
    auto &frame = state.polygon_frame;
    const auto ticket = capture_world_polygon_level(frame);
    validate_world_polygon_level(frame, ticket);
    if (!state.texture_animations)
        throw FormatError("Session polygon submission has no loaded texture animation owner");
    const auto views = world_polygon_submission_lists(frame);
    const PolygonSubmissionLists lists{{views[0], views[1], views[2], views[3], views[4], views[5], views[6]},
                                       {views[7], views[8]}};
    const PolygonSubmissionInputs inputs{materials.names.materials.active_materials(),
                                         state.texture_animations->records(),
                                         state.texture_animation.sky,
                                         materials.names.sky_material_base,
                                         capabilities.depth,
                                         state.view_quality.second,
                                         state.view_quality.legacy_driver_restriction,
                                         capabilities.multitexture,
                                         state.viewport};
    try
    {
        (void)prepare_polygon_submission_capacity(lists, inputs, scratch, budget);
        if (world_join)
            world_join->before_submission(session, lists, inputs, sink);
        submit_world_polygons(lists, inputs, scratch, sink);
        if (world_join)
            world_join->after_submission(sink);
    }
    catch (...)
    {
        // Host failure is terminal for this level. Keep completed source
        // writes; a caller cannot repeat an already advanced geometry frame.
        detach_world_polygon_level(frame);
        throw;
    }
}
} // namespace sgn::retail
