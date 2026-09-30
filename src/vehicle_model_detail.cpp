#include "sgn/vehicle_model_detail.hpp"
#include <algorithm>
#include <cmath>
namespace sgn::retail
{
std::uint32_t vehicle_model_detail_level(VehicleVector position, const CameraState &camera, CameraVector forward,
                                         std::uint32_t rate_selector)
{
    const double x = double(position.x) - camera.position.x, y = double(position.y) - camera.position.y,
                 z = double(position.z) - camera.position.z;
    const double distance = (((z * forward.z + y * forward.y) + x * forward.x) * double(3.2f)) / camera.lens;
    if (!std::isfinite(distance))
        throw FormatError("model detail distance outside finite source domain");
    // No binary32 store occurs before the source comparisons.
    std::uint32_t detail = distance <= 5000 ? (distance <= 3000 ? 0u : 1u) : (distance <= 10000 ? 2u : 3u);
    if (rate_selector < 25)
        detail += 2;
    else if (rate_selector < 50)
        ++detail;
    return std::min(detail, 3u);
}
void hide_distant_vehicle_details(VehicleState &vehicle, VehiclePresentationState &presentation,
                                  contact::effects::VehicleParticleState &particles,
                                  VehicleModelEffectResource &resource)
{
    const auto lod = resource.selected_lod();
    vehicle.brake_signal = 0;
    for (auto &wheel : vehicle.wheels)
        wheel.braking_signal = 0;
    const auto count = resource.part_count();
    for (auto part : {5u, 6u, 7u, 8u, 9u, 11u, 12u, 13u, 14u, 15u, 16u})
        resource.part(lod, part).flags = 0;
    if (count > 19)
        resource.part(lod, 19).flags = 0;
    if (count > 20)
        resource.part(lod, 20).flags = 0;
    particles.field_7628 = 0;
    presentation.field_3928 = 0;
    presentation.field_33c8 = 0;
    presentation.field_3508 = 0;
}
void prepare_vehicle_model_detail(VehicleState &vehicle, VehiclePresentationState &presentation,
                                  contact::effects::VehicleParticleState &particles,
                                  const VehiclePresentationEffects &effects, ModelResource &model,
                                  std::uint32_t &selected_lod, const CameraState &camera, CameraVector forward,
                                  std::uint32_t rate_selector)
{
    model.previous_detail_level() =
        vehicle_model_detail_level(vehicle.previous_position, camera, forward, rate_selector);
    selected_lod = vehicle_model_detail_level(vehicle.position, camera, forward, rate_selector);
    if (selected_lod >= 3)
    {
        VehicleModelEffectResourceView resource(model, selected_lod);
        hide_distant_vehicle_details(vehicle, presentation, particles, resource);
    }
    else
        prepare_vehicle_presentation(presentation, effects,
                                     model.presentation_parts(model.source_lod_index(selected_lod)));
}
namespace
{
void source_effect_scale(SourcePmdModelView model, std::uint32_t offset, Vec3 scale)
{
    // 416751/41675A/416763 and416783/416792/416795: three scalar stores.
    //  Preserve scale W and every other word of the same pointed transform.
    model.bytes().field<float>(offset + 0x10).set(scale.x);
    model.bytes().field<float>(offset + 0x14).set(scale.y);
    model.bytes().field<float>(offset + 0x18).set(scale.z);
}
void source_effect_parts(SourcePmdModelView model, unsigned lod, const VehiclePresentationEffects &effects)
{
    if (effects.field_226c == 0)
    {
        if (effects.field_328c == 1)
            model.part(lod, 19).flags().set(0);
        if (effects.field_33cc == 1)
            model.part(lod, 20).flags().set(0);
    }
    else if (effects.field_328c == 1)
    {
        const auto first = model.part(lod, 19);
        const auto offset = first.transform().location().offset; // 41673E before flag
        first.flags().set(1);
        source_effect_scale(model, offset, effects.first_scale);
        // 416736..41673C,416766..41676E: nested exact-one availability tests.
        if (effects.field_33cc == 1)
        {
            const auto second = model.part(lod, 20);
            second.flags().set(1);
            source_effect_scale(model, second.transform().location().offset, effects.second_scale);
        }
    }
}
} // namespace
namespace
{
template <class Acquire>
void source_presentation(VehiclePresentationState &presentation, const VehiclePresentationEffects &effects,
                         Acquire acquire)
{
    presentation.field_3134 = 1;  // 413DB2 before current5C/rawhead resolution
    const auto model = acquire(); // first PMD borrow follows the source scalar prefix
    const auto lod = model.source_lod_index(model.current_detail_level().value());
    for (unsigned part = 11; part <= 16; ++part)
        model.part(lod, part).flags().set(1);
    presentation.field_3928 = presentation.field_3924;
    source_effect_parts(model, lod, effects); // same checked4166F0 already qualified by detail
    presentation.field_33c8 = 1;
    presentation.field_3508 = 1;
}
} // namespace
void prepare_vehicle_presentation(VehiclePresentationState &presentation, const VehiclePresentationEffects &effects,
                                  SourcePmdModelView model)
{
    source_presentation(presentation, effects, [&] { return model; });
}
void prepare_vehicle_presentation(VehiclePresentationState &presentation, const VehiclePresentationEffects &effects,
                                  ModelResources &models, std::size_t handle)
{
    if (models.source_backed(handle))
        source_presentation(presentation, effects, [&] { return models.source_at(handle); });
    else
    {
        auto &model = models.at(handle);
        prepare_vehicle_presentation(
            presentation, effects,
            model.presentation_parts(model.source_lod_index(models.current_detail_level(handle))));
    }
}
void prepare_vehicle_model_detail(VehicleState &vehicle, VehiclePresentationState &presentation,
                                  contact::effects::VehicleParticleState &particles,
                                  const VehiclePresentationEffects &effects, SourcePmdModelView model,
                                  const CameraState &camera, CameraVector forward, std::uint32_t rate_selector)
{
    model.previous_detail_level().set(
        vehicle_model_detail_level(vehicle.previous_position, camera, forward, rate_selector));
    model.current_detail_level().set(vehicle_model_detail_level(vehicle.position, camera, forward, rate_selector));
    const auto selected = model.current_detail_level().value();
    const auto lod = model.source_lod_index(selected); // raw slot may alias an earlier relocated part table
    if (selected >= 3)
    { // full413CE0
        vehicle.brake_signal = 0;
        for (auto &wheel : vehicle.wheels)
            wheel.braking_signal = 0;
        const auto count = model.part_count();
        for (auto part : {5u, 6u, 7u, 8u, 9u, 11u, 12u, 13u, 14u, 15u, 16u})
            model.part(lod, part).flags().set(0);
        if (count > 19)
            model.part(lod, 19).flags().set(0);
        if (count > 20)
            model.part(lod, 20).flags().set(0);
        particles.field_7628 = 0;
        presentation.field_3928 = 0;
        presentation.field_33c8 = 0;
        presentation.field_3508 = 0;
    }
    else
    { // full413DA0 ->4166F0
        prepare_vehicle_presentation(presentation, effects, model);
    }
}
void prepare_vehicle_model_detail(VehicleState &vehicle, VehiclePresentationState &presentation,
                                  contact::effects::VehicleParticleState &particles,
                                  const VehiclePresentationEffects &effects, ModelResources &models, std::size_t handle,
                                  const CameraState &camera, CameraVector forward, std::uint32_t rate_selector)
{
    if (models.source_backed(handle))
        prepare_vehicle_model_detail(vehicle, presentation, particles, effects, models.source_at(handle), camera,
                                     forward, rate_selector);
    else
    {
        auto &model = models.at(handle);
        prepare_vehicle_model_detail(vehicle, presentation, particles, effects, model, model.current_detail_level(),
                                     camera, forward, rate_selector);
    }
}
} // namespace sgn::retail
