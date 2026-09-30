#pragma once
#include "sgn/source_vehicle_cold_tail.hpp"
#include "sgn/source_vehicle_uninterpreted_storage.hpp"
#include "sgn/retail_cold_start.hpp"
#include "sgn/retail_time_trial.hpp"
#include "sgn/retail_race_start.hpp"
#include "sgn/retail_vehicle_load.hpp"
#include "sgn/retail_vehicle_anchors.hpp"
#include "sgn/retail_vehicle_strip.hpp"
#include "sgn/vehicle_audio_state.hpp"
#include "sgn/vehicle_air_tick.hpp"
#include "sgn/contact_ai_stunts.hpp"
#include "sgn/contact_rail_state.hpp"
#include "sgn/vehicle_camera_settings.hpp"
#include "sgn/vehicle_configuration_identity.hpp"
#include "sgn/vehicle_restart.hpp"
#include "sgn/vehicle_segments.hpp"
#include "sgn/vehicle_initialization_extras.hpp"
#include "sgn/vehicle_tick.hpp"
#include "sgn/vehicle_model_effects.hpp"
#include "sgn/contact_vehicle_particle_state.hpp"
#include "sgn/vehicle_skid_strip.hpp"
#include "sgn/vehicle_tick_routes.hpp"
#include "sgn/vehicle_shadow.hpp"
#include "sgn/vehicle_progression.hpp"
#include "sgn/vehicle_race_startup_state.hpp"

namespace sgn::retail
{
// One owner for each represented per-vehicle field. No global clocks, RNG,
// replay/particle/popup banks, controller state, cached routes or shared matrices
// are stored here. New recovered per-car owners must join this inventory before
// the assembled full-world clear can be qualified.
class VehicleStorage final : public VehicleStorageClear
{
  public:
    static constexpr unsigned physical_slots = 6, presentation_slots = 8;
    std::array<VehicleParameters, physical_slots> parameters{};
    std::array<VehicleState, physical_slots> physics{};
    std::array<VehicleRaceState, physical_slots> race{};
    std::array<contact::solver::State, physical_slots> contact{};
    std::array<VehicleRaceDriverSettings, physical_slots> drivers{};
    std::array<AiAutomaticMemory, physical_slots> ai{};
    std::array<AiStuntHelperState, physical_slots> ai_stunts{};
    std::array<VehiclePositionHistory, physical_slots> history{};
    std::array<VehicleLoadState, physical_slots> load{};
    std::array<VehicleAirTickState, physical_slots> air{};
    std::array<VehicleTrailStorage, physical_slots> trails{};
    std::array<VehicleTrailInputs, physical_slots> trail_inputs{};
    std::array<VehicleAnchorState, physical_slots> anchors{};
    std::array<VehicleStripHeaders, physical_slots> strips{};
    std::array<VehicleAudioState, physical_slots> audio{};
    std::array<VehicleCameraSettings, physical_slots> camera_settings{};
    std::array<VehicleRailState, physical_slots> rail{};
    std::array<VehicleRestartState, physical_slots> restart{};
    std::array<VehicleSegments, physical_slots> segments{};
    std::array<VehicleInitializationExtras, physical_slots> initialization_extras{};
    std::array<VehicleTickState, physical_slots> tick{};
    std::array<VehicleModelEffects, physical_slots> model_effects{};
    std::array<contact::effects::VehicleParticleState, physical_slots> particles{};
    std::array<VehicleSkidRings, physical_slots> skid_rings{};
    std::array<VehicleRouteAssistState, physical_slots> route_assist{};
    std::array<VehicleShadowState, physical_slots> shadows{};
    std::array<VehicleProgressionState, physical_slots> progression{};
    std::array<VehicleRaceStartupState, physical_slots> startup{};
    std::array<VehicleConfigurationIdentity, presentation_slots> configuration_identity{};
    std::array<VehiclePresentationState, presentation_slots> presentation{};
    std::array<VehicleModelStorage, presentation_slots> models{};
    std::array<std::array<VehicleWheelAnimation, 4>, presentation_slots> wheels{};
    // Only the six physical source records have these bytes. Presentation
    // slots6/7 are outside444DC9, but their represented lanes are cleared by40A305.
    std::array<SourceVehicleUninterpretedStorage, physical_slots> source_uninterpreted{};
    std::array<SourceVehicleColdTail, 2> cold_tail{}; // only missing stride6/7 source bytes

    void clear_vehicle_storage() override;
    // 413670's represented per-car copy. A non-null external object at726C
    // needs a shared object owner before cloning and is rejected atomically.
    // Source and target are physical slots; resource handles retain identity.
    void clone_vehicle(unsigned source, unsigned target);
    ColdNumericStorage cold_numeric(unsigned &current_slot);
    ColdPresentationStorage cold_presentation();
    std::array<ResetVehicle, physical_slots> reset_views();
    std::array<contact::solver::Vehicle, physical_slots> contact_views();
    std::array<VehicleLoadVehicle, physical_slots> load_views();
    std::array<AiAutomaticVehicle, physical_slots> automatic_views();
    std::array<VehicleWakeVehicle, physical_slots> wake_views();
    CameraVehicle camera_snapshot(unsigned slot) const;
    void store_camera_settings(unsigned slot, const CameraVehicle &snapshot);
};
} // namespace sgn::retail
