#pragma once
#include "sgn/vehicle_stunt_awards.hpp"

namespace sgn::retail::contact::effects
{
struct EffectParticleBank;
}
namespace sgn::retail
{
struct ModelTransform;
struct VehicleAirTickState
{
    std::uint32_t completed_airtime{};
}; // sole 7200 owner
struct VehicleAirTickContext
{
    contact::player::Controls &controls;
    std::uint32_t vehicle_id;
    VehicleScoreState &score;
    const VehicleStuntContext &stunt; // game_mode=B4, race_mode=B0
    const VehicleStuntRewards &rewards;
    VehicleRandom &random;
    VehicleStuntChainAudio &chain_audio;
    contact::effects::EffectParticleBank &particles;
    Vec3 unit_gravity;
    const ModelTransform &template_pose; // borrowed live4864E8
};
// Diagnostics describe actual branches. They do not select or suppress effects.
struct VehicleAirTickObservation
{
    bool leveled_incremental{}, orthogonalized{}, emitted_takeoff{};
};
// Full 00412470..0041266E composition. Requires the real 4171C0 particle module;
// declarations alone do not establish playable or whole-function qualification.
VehicleAirTickObservation vehicle_air_tick(const VehicleParameters &, VehicleState &, VehicleAirTickState &,
                                           const VehicleAirTickContext &);
} // namespace sgn::retail
