#include "sgn/vehicle_model_effects.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
void route_flags(SourcePmdModelView model, std::uint16_t value)
{
    // 415716..41572A /415756..41576A: visit literal slots0..3, including aliases.
    // Each raw head is loaded once per visit; part10 is written before part5.
    for (unsigned raw = 0; raw < 4; ++raw)
    {
        const auto resident = model.source_lod_index(raw);
        model.part(resident, 10).flags().set(value);
        model.part(resident, 5).flags().set(value);
    }
}
} // namespace
// Complete415770..4157E5 +4156E0/415730/414EF0/4152C0:85 instructions,289 bytes.
// The checked PMD access mechanism is new; branch/arithmetic/write order is source.
void update_vehicle_route_lights(VehicleState &s, const VehicleRaceState &race, VehicleTrailInputs &trails,
                                 VehicleModelEffects &effects, SourcePmdModelView model, VehicleRoutePresentation route)
{
    if (model.part_count() <= 10)
        return; // 41577C..415780: before divisor or any part/LOD access
    if (route.point_count == 0)
        throw FormatError("route lights require nonzero first spline point count");
    const auto position = std::bit_cast<std::int32_t>(race.spline_index);
    const std::uint32_t remainder =
        position < 0 ? route.point_count - ((0u - race.spline_index) % route.point_count)
                     : race.spline_index % route.point_count; // 415792..4157A6: NEG/DIV are unsigned32
    const auto index = (remainder << 7) / route.point_count;  // 4157A8..4157AD: SHL wraps before DIV
    if (index > 128)
        throw FormatError("route lights index outside source table boundary");
    const float light = index == 128 ? std::bit_cast<float>(route.ambience[0]) : route.lights[index];
    if (!std::isnan(light) && light != 0)
    { // 4157B1..4157C5: unordered comparison takes off branch
        if (trails.field_28a0 != 1)
        { // complete4156E0 on transition
            trails.field_28a0 = 1;
            s.camera_effects.values[4] = 0;
            s.camera_effects.values[5] = 0; // 414EF0:2F18,3130
            s.camera_effects.values[6] = 0;
            s.camera_effects.values[7] = 0; // 4152C0:2AD4,2CF0
            route_flags(model, 1);
        }
    }
    else if (trails.field_28a0 != 0)
    { // complete415730 off transition; no counter reset
        trails.field_28a0 = 0;
        route_flags(model, 0);
    }
    effects.route_envelope = index == 128 ? route.ambience_adjacent_word : route.ambience[index]; // 4157D1..4157DF
}
void update_vehicle_route_lights(VehicleState &s, const VehicleRaceState &race, VehicleTrailInputs &trails,
                                 VehicleModelEffects &effects, ModelResources &models, std::size_t handle,
                                 VehicleRoutePresentation route)
{
    if (models.source_backed(handle))
    {
        update_vehicle_route_lights(s, race, trails, effects, models.source_at(handle), route);
    }
    else
    {
        VehicleModelEffectResourceView legacy(models.at(handle), models.current_detail_level(handle));
        update_vehicle_route_lights(s, race, trails, effects, legacy, route);
    }
}
} // namespace sgn::retail
