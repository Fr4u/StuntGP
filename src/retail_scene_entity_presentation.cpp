#include "sgn/retail_scene_entity_presentation.hpp"

namespace sgn::retail
{
namespace
{
void defined_colors(contact::effects::EffectSpriteColorAudit *audit, std::uint32_t begin, std::uint32_t end)
{
    if (!audit)
        return;
    if (begin > end || end > audit->source_undefined.size())
        throw FormatError("scene color audit cursor range");
    // 418180 and416050 write colors from defined glow/profile fields. Only
    // their actual appended attributes replace earlier opaque color metadata.
    for (auto index = begin; index < end; ++index)
        audit->source_undefined[index] = false;
}
} // namespace
VehiclePresentationDraws prepare_scene_entity_presentation(SceneEntityPresentation frame)
{
    auto &v = frame.vehicles;
    const auto vehicle_begin = v.sprites.attribute_count;
    auto result = prepare_vehicle_presentation_frame(v); // 442500 CALL418180
    defined_colors(frame.effect_color_audit, vehicle_begin, v.sprites.attribute_count);
    contact::effects::append_effect_particle_sprites(frame.effects, v.sprites, v.camera, frame.animations,
                                                     frame.adjacent_angles,
                                                     frame.effect_color_audit); // 442505 CALL45E4F0
    // 418305 restored the current physical vehicle to slot0.416050 reads that
    // orientation even when another camera targets a different physical slot.
    const auto impact_begin = v.sprites.attribute_count;
    append_impact_particle_trails({v.profiles, v.staging, v.sprites, v.screen, v.viewport, v.camera,
                                   v.vehicles.physics[v.current_slot].orientation, v.split_setting},
                                  frame.impacts, frame.impact_draws); // 44250A CALL416050
    defined_colors(frame.effect_color_audit, impact_begin, v.sprites.attribute_count);
    return result; // 44250F JMP449D60;449D60 is an actual single RET
}
} // namespace sgn::retail
