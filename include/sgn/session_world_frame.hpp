#pragma once
#include "sgn/retail_scene_entity_presentation.hpp"
#include "sgn/session_world_geometry.hpp"
#include "sgn/session_world_submission.hpp"
#include "sgn/retail_camera_frame.hpp"

namespace sgn::retail
{
// Explicit current source resource inputs. The connected emitters publish
// animation_binding0; a nonzero binding needs its actual descriptor entry.
// Adjacent trig samples and color provenance must retain their source owner
// generation. They are not defaults derived from a display frame.
struct SessionWorldFrameResources
{
    std::span<const contact::effects::EffectAnimationTexture> effect_animations;
    std::span<const contact::effects::EffectAngleSample> adjacent_angles;
    contact::effects::EffectSpriteColorAudit *effect_colors{};
    LightGammaDevice *gamma{};
    SessionWorldGeometryAudit geometry_audit;
    PolygonSubmissionBudget submission_budget;
};
// Normal-view slice4425FC..44260B. The accepted source outer/world tick has
// already run. This binds full442500 and42E870 through the existing two-view
// camera order, then calls433F40 once. The HUD/screen/finalization suffix and
// physical presentation remain separate. No inspection-mesh consumer is used.
// Candidate pending combined Session qualification; not a released caller.
CameraFrameSnapshot prepare_session_world_frame(RaceSession &, const SessionWorldMaterials &,
                                                SessionPolygonCapabilities, PolygonSubmissionState &,
                                                PolygonSubmissionSink &, SessionWorldFrameResources);
} // namespace sgn::retail
