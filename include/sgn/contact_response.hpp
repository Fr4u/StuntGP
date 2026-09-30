#pragma once
#include "sgn/track_contact.hpp"

namespace sgn::retail::contact::response
{
// Recovered contact response procedures. Integration into a complete tick is separate.
struct ContactRig
{
    std::array<ContactProbe, 6> probes{};
    std::uint32_t count{};
    float origin_y_offset{};
    float bounding_radius{};
};

void center_probes(std::span<ContactProbe>, float &origin_y_offset);                // 00422200
void initialize_rig(std::uint32_t selector, const Vector4 &position, ContactRig &); // 004082D0
void place_rig(const Vector4 &position, Pose &, ContactBody &, ContactRig &);       // 00408640
void damp_incremental(Matrix4 &, std::span<const ContactProbe, 4>);                 // 00421E70
void settle_incremental(Matrix4 &, std::span<const ContactProbe, 4>,
                        bool *audit_payload_undefined = nullptr);                        // 00421FA0
void linear_friction(const Vector4 &velocity, Vec3 normal, float strength, Vec3 &delta); // 00408BC0
void angular_friction(const Vector4 &position, const ContactProbe &, const Matrix4 &incremental, Vec3 snapshot_up,
                      Vec3 normal, float strength, Matrix4 &delta); // 004089B0
void limit_incremental(Matrix4 &);                                  // 004092D0

struct ImpactCount
{
    std::int32_t value{}, capacity{};
}; // vehicle +2568/+256C; semantic name unverified
void add_impact_count(ImpactCount &, std::int32_t game_mode, float strength); // 00408D10
struct EnergyImpactParameters
{
    float minimum{}, maximum{}, scale{}, accumulated_limit{}, hold_ticks{}, upgrade_loss_modifier{};
};
struct EnergyImpactState
{
    std::int32_t exempt{}, upgrade_active{};
    float upgrade_condition{}, energy{}, accumulated_loss{};
    std::int32_t hold{};
};
void consume_impact_energy(EnergyImpactState &, const EnergyImpactParameters &, float strength); // 00408DC0 + 00405820
struct DirectionalImpact
{
    Vec3 accumulated{};
    float magnitude{};
};
void directional_impact(DirectionalImpact &, std::int32_t game_mode, Vec3 normal,
                        float strength); // 00408CB0 + 00413290
struct ImpactResponseState
{
    float base_com_height{}, effective_com_height{};
    std::int32_t reset_ticks{};
    std::uint32_t reset_2628{}, reset_262c{}; // semantic names not established
    float shock{};
    std::int32_t wheel_touched{}, body_touched{};
    float alignment{}, contact_load{}, effect_strength{};
};
// Only 00409664..0040985A; caller must still perform material/stunt effects.
void finish_impact_physical(ContactBody &, const ContactProbe &, unsigned index, Vector4 normal, Vec3 snapshot_up,
                            float probe_mass, Material, std::int32_t track_mode, ImpactAccumulator &, WheelContact &,
                            ImpactResponseState &);
} // namespace sgn::retail::contact::response
