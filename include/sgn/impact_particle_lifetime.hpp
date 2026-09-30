#pragma once
#include "sgn/contact_cast.hpp"
#include "sgn/vehicle_trail_append.hpp"

namespace sgn::retail::contact::effects
{
struct ImpactParticleDrawList
{
    // Normalized identity codec, not a host/source pointer. Valid particle
    // indices0..31 and published counts0..32 cannot collide with this sentinel.
    static constexpr std::uint32_t null_value = 0xffffffffu;
    std::array<std::uint32_t, 32> slots = [] {
        std::array<std::uint32_t, 32> value{};
        value.fill(null_value);
        return value;
    }();
    std::uint32_t count{null_value}; // PE zero source cursor; count0 means61B5A4
    [[nodiscard]] bool cursor_is_null() const noexcept
    {
        return count == null_value;
    }
    static std::uint32_t encode_slot(std::uint32_t index)
    {
        if (index == null_value)
            return 0;
        if (index >= 32)
            throw FormatError("impact source slot identity");
        return 0x612da4u + index * 0x440u;
    }
    static std::uint32_t decode_slot(std::uint32_t pointer)
    {
        if (pointer == 0)
            return null_value;
        if (pointer < 0x612da4u || pointer >= 0x61b5a4u || (pointer - 0x612da4u) % 0x440u != 0)
            throw FormatError("impact source slot pointer");
        return (pointer - 0x612da4u) / 0x440u;
    }
    static std::uint32_t encode_cursor(std::uint32_t published)
    {
        if (published == null_value)
            return 0;
        if (published > 32)
            throw FormatError("impact source cursor count");
        return 0x61b5a4u + published * 4u;
    }
    static std::uint32_t decode_cursor(std::uint32_t pointer)
    {
        if (pointer == 0)
            return null_value;
        if (pointer < 0x61b5a4u || pointer > 0x61b624u || (pointer - 0x61b5a4u) % 4u != 0)
            throw FormatError("impact source cursor pointer");
        return (pointer - 0x61b5a4u) / 4u;
    }
};
struct ImpactParticleLifetimeAudit
{
    ContactCastPayloadAudit query;
    std::array<std::array<bool, 64>, 32> trail_position{};
};
using ImpactTrailProfiles = std::array<VehicleTrailParameters, 9>;
ImpactTrailProfiles initial_impact_trail_profiles() noexcept; // original474AF8 +type*110
struct ImpactParticleLifetimeBindings
{
    ImpactParticles &particles;
    ImpactParticleDrawList &draw_list;
    solver::Workspace &workspace;
    ContactCastState &query;
    VehicleRandom &random;
    const TrackGeometry &track;
    Vec3 gravity;
    const VehicleTrailProjection &projection;
    const ImpactTrailProfiles &profiles;
    solver::PayloadAudit *payload_audit{};
    ImpactParticleLifetimeAudit *audit{};
};
// Additive full4215B0 with persistent query state. Uses source face-only query,
// first accepted polygon, retained start on hit and original incremental steps.
std::uint32_t cast_track_particle_sphere(const TrackGeometry &, solver::Workspace &, ContactCastState &,
                                         ContactCastPayloadAudit *audit = nullptr,
                                         solver::PayloadAudit *payload_audit = nullptr,
                                         ContactQueryInternalsAudit *internals_audit = nullptr);
// Full415DF0, including up to9 reflection responses,401D70,42F1C0 and41E6E0.
// Publishes the expiry tick too. The existing32-slot bank owns the trail ring.
void update_impact_particles(ImpactParticleLifetimeBindings);
} // namespace sgn::retail::contact::effects
