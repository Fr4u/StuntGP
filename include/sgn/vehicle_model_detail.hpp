#pragma once
#include "sgn/vehicle_model_effects.hpp"
#include "sgn/contact_vehicle_particle_state.hpp"
#include "sgn/retail_camera.hpp"
namespace sgn::retail
{
// Standalone full413DA0 ->4166F0, reused by detail and the actual418855 tick call.
// Current5C/rawslot and ordered checked writes remain in the existing PMD owner.
void prepare_vehicle_presentation(VehiclePresentationState &, const VehiclePresentationEffects &, SourcePmdModelView);
void prepare_vehicle_presentation(VehiclePresentationState &, const VehiclePresentationEffects &, ModelResources &,
                                  std::size_t handle);
// Literal42B580 classification; camera_forward is the current9C..A4 basis,
// supplied by CameraPresentationState, not the camera target point.
std::uint32_t vehicle_model_detail_level(VehicleVector position, const CameraState &, CameraVector camera_forward,
                                         std::uint32_t rate_selector); // 61297C, shared with view quality
void hide_distant_vehicle_details(VehicleState &, VehiclePresentationState &, contact::effects::VehicleParticleState &,
                                  VehicleModelEffectResource &); // full413CE0
void prepare_vehicle_model_detail(
    VehicleState &, VehiclePresentationState &, contact::effects::VehicleParticleState &,
    const VehiclePresentationEffects &, ModelResource &, std::uint32_t &selected_lod, const CameraState &,
    CameraVector camera_forward,
    std::uint32_t rate_selector); // full413E10; model owns previous+64, caller owns selected+5C
// Canonical413E10 including full413CE0/413DA0/4166F0. Both LOD fields,
// flags and scale XYZ stay in the single bank allocation; no retained payload.
void prepare_vehicle_model_detail(VehicleState &, VehiclePresentationState &, contact::effects::VehicleParticleState &,
                                  const VehiclePresentationEffects &, SourcePmdModelView, const CameraState &,
                                  CameraVector camera_forward, std::uint32_t rate_selector);
// Actual consumer dispatch. Legacy uses its existing bound current field;
// canonical uses descriptor+5C directly, without Session selected_lod aliases.
void prepare_vehicle_model_detail(VehicleState &, VehiclePresentationState &, contact::effects::VehicleParticleState &,
                                  const VehiclePresentationEffects &, ModelResources &, std::size_t handle,
                                  const CameraState &, CameraVector camera_forward, std::uint32_t rate_selector);
} // namespace sgn::retail
