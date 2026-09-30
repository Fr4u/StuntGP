#pragma once
#include "sgn/vehicle_ai_automatic.hpp"
#include "sgn/vehicle_audio_state.hpp"

namespace sgn::retail
{
struct VehicleRouteAssistState
{
    float blend{};
}; // 265C; separate from AI266C
struct VehicleRouteAssistSettings
{
    std::uint32_t phase{};                                                            // 60F2B4
    Vec3 unit_gravity{};                                                              // 482688
    float upgraded_skill_threshold{};                                                 // 60F348
    std::array<float, 4> bend_speed{0, 125, 108.33333587646484f, 66.66666412353516f}; // 472B78
    std::array<float, 4> steering_gain{0, 0.05f, 0.1f, 0.2f};                         // 472B88
    float mass_override_gain{0.5f};                                                   // 472B98
};
struct VehicleRouteAssistContext
{
    AiAutomaticGlobals &routes;
    const AiAutomaticMemory &ai;
    const VehicleAudioState &selectors; // 771C/7720 single owner
    float driver_skill;                 // 1B8C
    const VehicleRouteAssistSettings &settings;
};
void limit_vehicle_route_speed(const VehicleParameters &, VehicleState &, float target_speed); // 405BE0
void steer_vehicle_route_target(const VehicleParameters &, VehicleState &, const AiAutomaticMemory &, Vec3 target,
                                float gain, const VehicleRouteAssistSettings &); // 4054B0
void steer_vehicle_override_target(const VehicleParameters &, VehicleState &, Vec3 target, float gain,
                                   Vec3 unit_gravity); // 406020
void guide_vehicle_route(const VehicleParameters &, VehicleState &, const VehicleRouteAssistState &,
                         const VehicleRouteAssistContext &, float gain); // 405610
void guide_vehicle_override(const VehicleParameters &, VehicleState &, const VehicleRouteAssistContext &,
                            float gain); // 406140
void update_vehicle_route_braking(const VehicleParameters &, VehicleState &, VehicleRaceState &,
                                  VehicleRouteAssistState &,
                                  const VehicleRouteAssistContext &); // 405C80
void update_vehicle_route_guidance(const VehicleParameters &, VehicleState &, VehicleRaceState &,
                                   const VehicleRouteAssistState &,
                                   const VehicleRouteAssistContext &); // 406190
} // namespace sgn::retail
