#pragma once
#include "sgn/session_world_frame.hpp"
#include <optional>

namespace sgn::retail
{
struct RaceTextureReplacement;
enum class SessionSceneFrameKind
{
    normal,
    race_finished,
    no_player
};
struct SessionSceneFrameResult
{
    SessionSceneFrameKind kind;
    std::optional<CameraFrameSnapshot> camera; // present only on the normal path
    std::uint32_t screen_records{};            // read-only host observation before432F60 clears count
};
// Candidate complete4425D0 assembly. Source outer/world tick already ran.
// One persistent Session UI owner, physical material bank and submission sink;
// no Begin/EndScene, presentation, outer442640 or source clock advancement.
// Run once per accepted source frame, then replay immutable GPU commands.
SessionSceneFrameResult prepare_session_scene_frame(RaceSession &, const SessionWorldMaterials &,
                                                    SessionPolygonCapabilities, PolygonSubmissionState &,
                                                    PolygonSubmissionSink &, SessionWorldFrameResources,
                                                    const RaceTextureReplacement &);
} // namespace sgn::retail
