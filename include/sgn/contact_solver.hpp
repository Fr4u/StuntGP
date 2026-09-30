#pragma once
#include "sgn/contact_effects.hpp"
#include "sgn/contact_response.hpp"
#include "sgn/contact_surface.hpp"
#include "sgn/track_collision.hpp"
#include "sgn/contact_payload_audit.hpp"

namespace sgn::retail::contact
{
struct ContactCastState;
struct ContactCastPayloadAudit;
struct ContactQueryInternalsAudit;
} // namespace sgn::retail::contact
namespace sgn::retail::contact::solver
{
struct WheelSurfaceHistory
{
    float current_load{};
    std::int32_t previous_type{}, type{}, changed{};
};
// Persistent contact storage; shared physical values remain in VehicleState.
struct State
{
    ContactBody body{};
    response::ContactRig rig{};
    Matrix4 orientation_snapshot{};
    Matrix4 matrix_7338{}; // initialized by 004085E0; later role remains unverified
    std::array<WheelSurfaceHistory, 4> wheels{};
    effects::PairCounters pair_counters{};
    surface::State surface{};
    response::ImpactCount impact_count{};
    float accumulated_energy_loss{}, direction_magnitude{}, shock{}, pair_shock{};
    std::int32_t energy_hold{}, shock_hold{}, wheel_touched{}, com_reset_ticks{};
    std::uint32_t com_2628{}, com_262c{}, pair_effect_ticks{};
    std::uint32_t effect_2368{}; // single owner; 0041A9E0 writes 16 for replay pair shock
};
struct Vehicle
{
    VehicleParameters &parameters;
    VehicleState &state;
    VehicleRaceState &race;
    State &contact;
    player::VehicleId id{};
};
struct Parameters
{
    float gravity{};
    std::array<float, 128> restitution{};
    response::EnergyImpactParameters energy;
    std::int32_t game_mode{}, race_mode{}, track_mode{};
    // Static data from pinned PE 00472C20, used by 00409C6C.
    Matrix4 inertia{1, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 1};
};
struct Workspace
{
    ImpactAccumulator impact{};
    response::ImpactResponseState response{};
    Vector4 last_normal{};
    Vec3 query_motion{}; // original global 004980E8, survives the probe query
    float query_mass{};  // original shared4980C0;409D26 writes body.mass, camera/particles write1
    std::optional<unsigned> selected_partner;
    std::optional<unsigned> source_body_vehicle_49a178; // sole published source body identity
    Vec3 source_position_sum_49a18c{};                  // sole XYZ accumulator, W remains average_payload
    // Actual409D1C/35/81/8E publications, split native owner identities.
    std::optional<unsigned> source_query_incremental_vehicle_4980e4;
    std::optional<unsigned> source_query_body_vehicle_4981cc;
    struct SourceProbeIdentity
    {
        unsigned vehicle{}, probe{};
    };
    std::optional<SourceProbeIdentity> source_query_probe_4981d0;
    std::optional<unsigned> source_query_vehicle_498228;
    std::uint32_t average_payload{}; // original global 0049A198
    float inverse_probe_count{};
};
struct World
{
    std::span<Vehicle, 6> vehicles;
    const TrackGeometry &geometry;
    const Parameters &parameters;
    player::Controls &controls;
    effects::ImpactParticles &particles;
    VehicleRandom &random;
    surface::InvertedContact &inverted;
    Workspace &workspace;
    ContactCastState &cast; // same persistent owner as camera, shadow and particles
    PayloadAudit *audit{};  // optional test-only source definedness; never drives physical state
    ContactCastPayloadAudit *cast_audit{};
    ContactQueryInternalsAudit *internals_audit{};
};
// Full call graph implementation candidate. Qualification against complete
// original steps is required before connecting this to a playable runtime.
int step(World &, unsigned vehicle_index);        // 00409B60
void initialize(State &, std::uint32_t selector); // 004085E0 and 004082D0
void impact(World &, unsigned vehicle_index, unsigned probe_index, Vector4 normal, Material, Vec3 &motion); // 004093E0
void collide_pair(World &, unsigned first, unsigned second);                                                // 004098A0
void place(Vehicle &, Vector4 position); // 00408640 adapter
VehicleSupport support(const State &);
} // namespace sgn::retail::contact::solver
