#include "sgn/vehicle_draw_pose.hpp"

namespace sgn::retail
{
void update_vehicle_draw_ambience(const VehicleModelEffects &effects, SourcePmdModelView model)
{
    const auto value = vehicle_draw_ambience(effects); // 418157..418163, including462860
    model.ambience().set(value);                       // 418168..41816F; only the low WORD is written
}
void update_vehicle_draw_ambience(const VehicleModelEffects &effects, ModelResources &models, std::size_t handle)
{
    if (models.source_backed(handle))
        update_vehicle_draw_ambience(effects, models.source_at(handle));
    else
        models.at(handle).source_model_ambience() = vehicle_draw_ambience(effects);
}
void append_vehicle_model_draw(const VehicleState &vehicle, float contact_origin_y, const VehicleModelEffects &effects,
                               const VehicleRaceState &race, VehicleModelStorage &storage, ModelResources &models,
                               std::size_t handle, ModelDrawQueue &regular, ViewMatrixArena &matrices,
                               const ModelTransform &template_pose)
{
    auto pose = vehicle_draw_pose(vehicle, contact_origin_y, template_pose); // 4181E1..41829B
    update_vehicle_draw_ambience(effects, models, handle);                   // 41829C -> full418150
    update_vehicle_draw_flags(race, storage);                                // 4182A1..4182CE
    append_model_draw(regular, models, handle, models.current_detail_level(handle), pose, storage.field_2708,
                      matrices); // 4182CF..4182E3
}
} // namespace sgn::retail
