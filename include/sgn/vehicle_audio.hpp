#pragma once
#include "sgn/retail_audio_driver.hpp"
#include "sgn/retail_vehicle_load.hpp"
#include "sgn/contact_solver.hpp"
#include "sgn/vehicle_audio_state.hpp"
#include <span>
namespace sgn::retail
{
struct VehicleAudioGlobals
{
    std::uint32_t controller_gain{}; // 4746C0, exact100 selects idle path
    float motion_gain{};             // 4747B8, shared across cars
};
struct VehicleAudioCar
{
    const VehicleParameters &parameters;
    VehicleState &physics;
    const VehicleRaceState &race;
    contact::solver::State &contact;
    const VehicleLoadState &load;
    VehicleAudioState &audio;
    contact::player::VehicleId id{};
};
struct VehicleAudioBindings
{
    VehicleAudioCar &car;
    AudioDriver &driver;
    contact::player::Controls &controls; // same owner as driver.controls
    VehicleAudioGlobals &globals;
    VehicleAudioFrontier &frontier;
};
struct VehicleAudioWorld
{
    std::span<VehicleAudioCar, 6> cars;
    AudioDriver &driver;
    contact::player::Controls &controls;
    VehicleAudioGlobals &globals;
    VehicleAudioFrontier &frontier;
    const std::uint32_t &pause_flag; // 4D5B20, exactly1
};
void vehicle_audio_grip(VehicleAudioBindings);                            // 41AF70
void vehicle_audio_rolling(VehicleAudioBindings);                         // 41B0D0
void vehicle_audio_landing(VehicleAudioBindings);                         // 41B250
void vehicle_audio_scrape(VehicleAudioBindings);                          // 41B2F0
void vehicle_audio_collision(VehicleAudioBindings);                       // 41B510
void vehicle_audio_boost(VehicleAudioBindings);                           // 41B5D0
void vehicle_audio_surface_event(VehicleAudioBindings);                   // 41B680
void vehicle_audio_events(VehicleAudioBindings);                          // 41B710
void vehicle_audio_motion(VehicleAudioBindings);                          // 41B8E0
void vehicle_audio_engine(VehicleAudioBindings);                          // 41BA40
void vehicle_audio_steering(VehicleAudioBindings);                        // 41BC80
void reset_vehicle_audio(VehicleAudioWorld &);                            // 41BDD0, clears exact233C..23A0
void update_vehicle_audio(VehicleAudioWorld &, unsigned current_vehicle); // 41BEA0, full caller
} // namespace sgn::retail
