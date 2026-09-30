#pragma once
#include "sgn/contact_cast.hpp"
#include "sgn/vehicle_presentation.hpp"
namespace sgn::retail
{
struct VehicleShadowState
{
    float factor_392c{}, factor_3930{};
    contact::Vector4 position{}, normal{}, forward{}; // 3934,3944,3954
};
struct VehicleShadowLighting
{
    const std::uint32_t &active_light_count; // 62452C, actual42D9B0 current-view count
    const contact::Vector4 &position;        // selected PMD light589E04 +10, no duplicated owner
};
// Shadow availability comes from the existing model-resource handle, never a
// second shadow model. Both query scratch and the contact workspace are shared.
void update_vehicle_shadow(const VehicleState &, VehiclePresentationState &, VehicleShadowState &,
                           bool shadow_available, const VehicleShadowLighting &, std::uint32_t source_tick,
                           contact::player::VehicleId, const contact::player::Controls &, Vec3 gravity,
                           const contact::TrackGeometry &, contact::solver::Workspace &, contact::ContactCastState &,
                           contact::ContactCastPayloadAudit *audit = nullptr); // full41BF20+402590+4212A0
} // namespace sgn::retail
