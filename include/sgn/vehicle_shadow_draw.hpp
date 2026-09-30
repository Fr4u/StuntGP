#pragma once
#include "sgn/retail_model_draw_queue.hpp"
#include "sgn/vehicle_shadow.hpp"

namespace sgn::retail
{
// Complete41C420, requiring the actual shadow resource and background queue.
// No visibility override; false is the original3928zero early return.
bool append_vehicle_shadow_draw(const VehiclePresentationState &, const VehicleShadowState &,
                                ModelResource &vehicle_model, ModelResource &shadow_model, unsigned shadow_source_lod,
                                std::uint32_t shadow_identity, ModelDrawQueue &background, ViewMatrixArena &,
                                const ModelTransform &template_pose);
// Canonical41C420 reads the shadow's own current LOD+5C; no separate selector.
// Attribute and post-queue descriptor writes use checked same-payload fields.
bool append_vehicle_shadow_draw(const VehiclePresentationState &, const VehicleShadowState &,
                                SourcePmdModelView vehicle_model, SourcePmdModelView shadow_model,
                                std::uint32_t shadow_identity, ModelDrawQueue &background, ViewMatrixArena &,
                                const ModelTransform &template_pose);
// Visibility returns before registry lookup, as the source3928 gate does.
bool append_vehicle_shadow_draw(const VehiclePresentationState &, const VehicleShadowState &, ModelResources &,
                                std::size_t vehicle_handle, std::size_t shadow_handle, ModelDrawQueue &background,
                                ViewMatrixArena &, const ModelTransform &template_pose);
} // namespace sgn::retail
