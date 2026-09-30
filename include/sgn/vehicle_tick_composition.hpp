#pragma once
#include "sgn/contact_engine_particles.hpp"
#include "sgn/contact_rail.hpp"
#include "sgn/vehicle_dynamics_tick.hpp"
#include "sgn/vehicle_audio.hpp"
#include "sgn/vehicle_model_effects.hpp"
#include "sgn/vehicle_progression.hpp"
#include "sgn/vehicle_reset_tick.hpp"
#include "sgn/vehicle_shadow.hpp"
#include "sgn/vehicle_skid_strip.hpp"
#include "sgn/vehicle_tick.hpp"
#include "sgn/vehicle_tick_routes.hpp"
#include "sgn/vehicle_ground_stunts.hpp"
#include "sgn/vehicle_prestart_progression.hpp"
#include "sgn/vehicle_normal_progression.hpp"

namespace sgn::retail
{
class VehicleStorage;
struct VehicleTickProjectionGlobals
{
    VehicleMatrix &projection;            // 61C3E8
    float &inverse_depth_scale;           // 61C534
    float &depth_offset;                  // 61C538
    VehicleMatrix &shared_model_identity; // 61C468, final44F4C0 writer
};
// NEW R18 (DESIGN A7): the 418945/4189BB skid-strip stage of a world whose
// strip geometry is source-allocation bound (VehicleStripGeometry::source,
// drawn by source 41D1C0). When bound it replaces update_vehicle_skid; it
// reads the current physical slot the traversal has already selected.
struct VehicleStripStage
{
    virtual ~VehicleStripStage() = default;
    virtual void update(unsigned current) = 0;
};
// NEW R21: the six particle children of 418830 (418922..41893B) and of the
// replay step 418970 (41899D..4189B6). When bound, each call site dispatches to
// the source-world translation (416A80, 415880, 417CA0, 416F30, 417800, 4173E0)
// instead of the contact::effects adapters; the caller keeps the original order.
enum class VehicleParticleChild
{
    engine,
    tire,
    load,
    landing,
    surface,
    launch_wheel
};
struct VehicleParticleStage
{
    virtual ~VehicleParticleStage() = default;
    virtual void emit(VehicleParticleChild child, unsigned current) = 0;
};
// Borrowed per-call view, not a world owner. The caller keeps every referenced
// object alive; validate_vehicle_tick_bindings rejects split identities before
// any source mutation. VehicleResetWorld.current selects the physical slot.
struct VehicleTickWorld
{
    VehicleStorage &vehicles;
    contact::solver::World &contact;
    RailWorld &rail;
    const VehicleProgressionContext &progression;
    const VehicleDynamicsTickContext &dynamics;
    AiAutomaticGlobals &automatic;
    const AiAutomaticEnvironment &automatic_environment;
    VehicleAudioWorld &audio;
    ModelResources &models;
    std::span<const unsigned, 6> selected_lod;
    ReplayBank &replays;
    ReplayWorkspace &recording;
    const WorldGravity &gravity;
    VehicleMatrix &model_basis; // 4D5B30, one world matrix
    const VehicleTrailProjection &trail_projection;
    const VehicleTrailSettings &trail_settings;
    VehicleStripGeometry &strip;
    contact::effects::EffectStaging &effect_staging; // 4D5BC0/4D5C94, one world store
    const VehicleShadowLighting &shadow_lighting;
    contact::ContactCastState &cast;
    VehicleRaceFinishWorld *prestart_finish{}; // mandatory only while mode1 is live
    VehicleTickProjectionGlobals *projection_globals{};
    const AiPlayerPrelude *player_prelude{};
    AiRivalStep *rival_step{};
    NormalRaceProgression *normal_progression{};
    VehicleStripStage *strip_stage{};       // R18 A7: null keeps native update_vehicle_skid
    VehicleParticleStage *particle_stage{}; // R21: null keeps the contact::effects emitters
};
// Configuration values are snapshots of their sole world/level owners. Their
// lifetimes cover this source tick, and no value is derived from frame duration.
struct VehicleTickConfiguration
{
    const VehiclePerformanceSettings &performance;
    const VehicleRouteAssistSettings &route_assist;
    const VehicleResetBounds &reset_bounds;
    VehicleRoutePresentation route_presentation;
    std::uint32_t start_boost_score{};                  // 60D6EC
    std::string_view start_boost_text, low_energy_text; // 61287C/6127F8
    const VehicleGroundStuntRewards *ground_rewards{};  // mandatory in modes1/2/3; mode4 skips ground stunts
};
struct VehicleTickAudit
{
    contact::effects::EffectParticleFrameAudit *particle_frames{};
    VehicleSkidWriteAudit *skid{};
    contact::ContactCastPayloadAudit *cast{};
    VehicleMatrix *rotation_matrix{}; // source math scratch61C3A4, diagnostic only
    contact::solver::PayloadAudit *contact_payload{};
    void *call_context{};
    void (*before_call)(void *, std::uint32_t) noexcept {}; // diagnostic caller instruction
    VehicleTrailWriteAudit *trail_writes{};
    VehicleWheelPositionWriteAudit *wheel_positions{};
};
struct VehicleTickObservation
{
    VehicleProgressionObservation progression;
    VehicleAirTickObservation air;
    bool bounds_placement{};
};
void validate_vehicle_tick_bindings(const VehicleTickWorld &, const VehicleTickConfiguration &);
// Full418830 source order: TimeTrial modes1/4, normal race modes1/2,
// and challenge game_type3/runtime_modes1/3/lap_count100. Mode3 skips COM;
// modes1/2/3 retain all five ground stunt calls, while mode4 skips them.
VehicleTickObservation tick_time_trial_vehicle(VehicleTickWorld &, const VehicleTickConfiguration &,
                                               VehicleTickAudit *audit = nullptr);
} // namespace sgn::retail
