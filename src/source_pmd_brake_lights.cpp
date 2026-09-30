#include "sgn/vehicle_model_effects.hpp"
#include <bit>

namespace sgn::retail
{
// Pinned D3D PE: complete415640..4156D5, 43 instructions, no callees.
// Only the model access mechanism is new; scalar arithmetic/order is source.
namespace
{
template <class Acquire>
void source_brake_lights(VehicleState &s, const VehicleTrailInputs &trails, VehicleModelEffects &effects,
                         Acquire acquire)
{
    if (s.brake_signal != 0)
    { // 415645..415660: wrapping SUB32, signed clamp
        s.brake_signal = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(s.brake_signal) - 32u);
        if (s.brake_signal < 0)
            s.brake_signal = 0;
    }
    if (trails.field_28a0 == 1 && s.brake_signal < 120)
        s.brake_signal = 120;     // 415666..41567E
    const auto model = acquire(); // first PMD borrow follows the source scalar prefix
    const auto part = model.part(model.source_lod_index(model.current_detail_level().value()), 5); // 415684..41569A
    const auto current = std::bit_cast<std::uint32_t>(s.brake_signal);
    part.flags().set(static_cast<std::uint16_t>(current != 0)); // 4156A0..4156A5, exactly WORD+00
    if (current == effects.cached_brake)
        return;                                        // 4156A8..4156B0, no attributes touched
    effects.cached_brake = current;                    // 4156B4, before count/pointer access
    const auto count = part.attribute_count().value(); // 4156BA, low16 only
    for (std::uint32_t i = 0; i < count; ++i)
    { // 4156C1..4156D2, zero count never dereferences+34
        part.attribute_byte(i, 2).set(static_cast<std::uint8_t>(current & 0xffu));
        part.attribute_byte(i, 3).set(0x60);
    }
}
} // namespace
void update_vehicle_brake_lights(VehicleState &s, const VehicleTrailInputs &trails, VehicleModelEffects &effects,
                                 SourcePmdModelView model)
{
    source_brake_lights(s, trails, effects, [&] { return model; });
}
void update_vehicle_brake_lights(VehicleState &s, const VehicleTrailInputs &trails, VehicleModelEffects &effects,
                                 ModelResources &models, std::size_t handle)
{
    if (models.source_backed(handle))
    {
        source_brake_lights(s, trails, effects, [&] { return models.source_at(handle); });
    }
    else
    {
        VehicleModelEffectResourceView legacy(models.at(handle), models.current_detail_level(handle));
        update_vehicle_brake_lights(s, trails, effects, legacy);
    }
}
} // namespace sgn::retail
