#pragma once
#include "sgn/source_cold_scene_startup.hpp"
namespace sgn::retail
{
struct LightGammaDevice;
// Actual42E791..42E7F8 after this scene's completed camera startup. Invocation-only
// same owners; caller admits current allocation/backend epoch and once-only stage.
// No new allocations, camera, cold history flags or material/draw revalidation.
// Reached source writes survive exceptions, including initialized6244F8=0.
// Track descriptor+0C publishes the actual shared4864E8 template identity.
void finish_source_scene_post_camera_42e791(SourceColdSceneStartupContext &,
                                            LightGammaDevice *const &source_gamma_5da1dc);
// Invocation-only same-world context from v1. Reached42E778, not a resume token.
// Requires actual material/font/count completion and concrete bootstrap receivers.
// Returns only after42E7F8, in the admitted cold scene2/generated-zero-polygon domain.
void finish_source_cold_scene_tail_42e778(SourceColdSceneStartupContext &,
                                          LightGammaDevice *const &source_gamma_5da1dc);
// Full cold42E6E0 composition: v1 prefix/materials then mandatory tail. It never
// initializes a page, changes scene2/target2 or duplicates442120/43D0B0 work.
void initialize_source_cold_scene_42e6e0(SourceColdSceneStartupContext &, LightGammaDevice *const &source_gamma_5da1dc);
} // namespace sgn::retail
