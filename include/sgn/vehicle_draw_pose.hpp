#pragma once
#include "sgn/vehicle_model_effects.hpp"
#include "sgn/retail_race.hpp"
#include "sgn/retail_model_draw_queue.hpp"

namespace sgn::retail
{
// 4181E1..41829C local80B pose. The height is contact.rig.origin_y_offset73B0,
//  not vehicle speed or suspension. Local forward denotes the target point.
ModelTransform vehicle_draw_pose(const VehicleState &, float contact_origin_y, const ModelTransform &template_pose);
std::uint16_t vehicle_draw_ambience(const VehicleModelEffects &);                         // full418150
void update_vehicle_draw_flags(const VehicleRaceState &, VehicleModelStorage &) noexcept; // 4182A1..4182CF
// Full418150: evaluate route7684*512 via462860, then write current descriptor04.
void update_vehicle_draw_ambience(const VehicleModelEffects &, SourcePmdModelView);
void update_vehicle_draw_ambience(const VehicleModelEffects &, ModelResources &, std::size_t handle);
// Closed4181E1..4182E3 only. Earlier trail calls and active-slot iteration are
// caller-owned. Copy the current template, ambience, blink, then42B850 using
// that model's current5C. No independent LOD or PMD payload snapshot is retained.
void append_vehicle_model_draw(const VehicleState &, float contact_origin_y, const VehicleModelEffects &,
                               const VehicleRaceState &, VehicleModelStorage &, ModelResources &, std::size_t handle,
                               ModelDrawQueue &regular, ViewMatrixArena &, const ModelTransform &template_pose);
} // namespace sgn::retail
