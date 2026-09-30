#include "sgn/retail_vehicle_storage.hpp"
#include <utility>

namespace sgn::retail
{
void VehicleStorage::clear_vehicle_storage()
{
    // 444DB0 clears exactly six original strides. The separate two presentation
    // slots and all external resource transforms remain untouched.
    for (unsigned i = 0; i < physical_slots; ++i)
    {
        parameters[i] = {};
        physics[i] = {};
        race[i] = {};
        contact[i] = {};
        drivers[i] = {};
        ai[i] = {};
        ai_stunts[i] = {};
        history[i] = {};
        load[i] = {};
        air[i] = {};
        trails[i] = {};
        trail_inputs[i] = {};
        anchors[i] = {};
        strips[i] = {};
        audio[i] = {};
        presentation[i] = {};
        models[i] = {};
        wheels[i] = {};
        camera_settings[i] = {};
        rail[i] = {};
        restart[i] = {};
        configuration_identity[i] = {};
        segments[i] = {};
        initialization_extras[i] = {};
        tick[i] = {};
        model_effects[i] = {};
        particles[i] = {};
        skid_rings[i] = {};
        route_assist[i] = {};
        shadows[i] = {};
        progression[i] = {};
        startup[i] = {};
    }
}
void VehicleStorage::clone_vehicle(unsigned source, unsigned target)
{
    if (source >= physical_slots || target >= physical_slots)
        throw std::out_of_range("vehicle clone physical slot");
    if (source == target)
        return;
    if (contact[source].surface.object_mode)
        throw std::domain_error("vehicle clone requires an explicit shared owner for source object726C");
    parameters[target] = parameters[source];
    physics[target] = physics[source];
    race[target] = race[source];
    contact[target] = contact[source];
    drivers[target] = drivers[source];
    ai[target] = ai[source];
    ai_stunts[target] = ai_stunts[source];
    history[target] = history[source];
    load[target] = load[source];
    air[target] = air[source];
    trails[target] = trails[source];
    trail_inputs[target] = trail_inputs[source];
    anchors[target] = anchors[source];
    strips[target] = strips[source];
    audio[target] = audio[source];
    presentation[target] = presentation[source];
    models[target] = models[source];
    wheels[target] = wheels[source];
    camera_settings[target] = camera_settings[source];
    rail[target] = rail[source];
    restart[target] = restart[source];
    configuration_identity[target] = configuration_identity[source];
    segments[target] = segments[source];
    initialization_extras[target] = initialization_extras[source];
    tick[target] = tick[source];
    model_effects[target] = model_effects[source];
    particles[target] = particles[source];
    skid_rings[target] = skid_rings[source];
    route_assist[target] = route_assist[source];
    shadows[target] = shadows[source];
    progression[target] = progression[source];
    startup[target] = startup[source];
}
ColdNumericStorage VehicleStorage::cold_numeric(unsigned &current_slot)
{
    return {parameters, physics, race, contact, current_slot};
}
ColdPresentationStorage VehicleStorage::cold_presentation()
{
    return {presentation, models, wheels};
}
std::array<ResetVehicle, 6> VehicleStorage::reset_views()
{
    return [this]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<ResetVehicle, 6>{{{physics[I], race[I], contact[I], static_cast<unsigned>(I)}...}};
    }(std::make_index_sequence<6>{});
}
std::array<contact::solver::Vehicle, 6> VehicleStorage::contact_views()
{
    return [this]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<contact::solver::Vehicle, 6>{
            {{parameters[I], physics[I], race[I], contact[I], static_cast<unsigned>(I)}...}};
    }(std::make_index_sequence<6>{});
}
std::array<VehicleLoadVehicle, 6> VehicleStorage::load_views()
{
    return [this]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<VehicleLoadVehicle, 6>{{{parameters[I], physics[I], race[I], history[I], load[I]}...}};
    }(std::make_index_sequence<6>{});
}
std::array<AiAutomaticVehicle, 6> VehicleStorage::automatic_views()
{
    return [this]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<AiAutomaticVehicle, 6>{{{parameters[I], physics[I], race[I], ai[I]}...}};
    }(std::make_index_sequence<6>{});
}
std::array<VehicleWakeVehicle, 6> VehicleStorage::wake_views()
{
    return [this]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<VehicleWakeVehicle, 6>{{{physics[I], race[I]}...}};
    }(std::make_index_sequence<6>{});
}
CameraVehicle VehicleStorage::camera_snapshot(unsigned slot) const
{
    CameraVehicle snapshot{};
    const auto &physical = physics.at(slot);
    const auto &record = race.at(slot);
    read_vehicle_camera_settings(snapshot, camera_settings.at(slot), record.saved_camera_mode);
    // Both readers refer to the same source word7238: stunt launch height.
    camera_observe_vehicle(snapshot, parameters.at(slot), physical,
                           {static_cast<std::uint32_t>(record.active), record.progress, physical.stunt.launch_height});
    return snapshot;
}
void VehicleStorage::store_camera_settings(unsigned slot, const CameraVehicle &snapshot)
{
    write_vehicle_camera_settings(camera_settings.at(slot), race.at(slot).saved_camera_mode, snapshot);
}
} // namespace sgn::retail
