#pragma once
#include "sgn/track_contact.hpp"
#include "sgn/vehicle_math.hpp"
#include <optional>

namespace sgn::retail::contact::effects
{
struct PairCounters
{
    std::array<std::uint32_t, 6> current{}, previous{}, count{};
};
struct PairVehicle
{
    Vector4 previous_position{};
    std::int32_t active{};
    std::uint32_t cooldown{};
    PairCounters counters{};
};
bool find_contact_partner(std::span<PairVehicle, 6>, unsigned current, std::int32_t mode,
                          std::optional<unsigned> &selected);                       // 00421B80
void mark_pair_contact(std::span<PairVehicle, 6>, unsigned first, unsigned second); // 0041D270
void update_pair_counters(PairCounters &);                                          // 0041D2C0
struct PairImpulse
{
    Vec3 direction{};
    float distance{}, normal_speed{};
};
// Physical interval only: 004098A0..00409A4B, before particle/energy/state effects.
PairImpulse apply_pair_impulse(ContactBody &first, ContactBody &second, Vector4 &first_pose_velocity,
                               Vector4 &second_pose_velocity);

struct ImpactParticle
{
    std::int32_t lifetime{}, type{};
    Vector4 position{}, velocity{};
    float decay{}, scale{};
    std::array<std::uint32_t, 256> reserved{}; // untouched by 00415AC0
    std::uint32_t field_430{}, field_434{};
    float alpha{};
    std::int32_t initial_lifetime{};
};
struct ImpactParticles
{
    std::uint32_t emitted{}, cursor{}, rate_selector{};
    std::int32_t throttle{};
    std::array<ImpactParticle, 32> particles{};
};
void emit_impact(ImpactParticles &, VehicleRandom &, const Vector4 &position, const Vector4 &velocity,
                 float strength); // full 00415AC0 and RNG callees
} // namespace sgn::retail::contact::effects
