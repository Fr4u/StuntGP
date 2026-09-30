#include "sgn/contact_surface.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::surface
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
} // namespace
bool attach_controller(player::Controller &controller, State &state, VehicleState &vehicle)
{
    const auto &position = vehicle.position;
    auto &camera = controller.camera;
    if (camera.mode >= 10)
        return false;
    if (state.object_mode && *state.object_mode == 2)
        return false;
    const double dx = double(position.x) - camera.position.x;
    const float dy = f32(double(position.y) - camera.position.y);
    const double extended_z = double(position.z) - camera.position.z;
    const float dz = f32(extended_z);
    const double distance = std::sqrt((extended_z * dz + double(dy) * dy) + dx * dx);
    if (distance < 500)
    {
        camera.velocity.x = 0;
        camera.velocity.y = 0;
        camera.velocity.z = 0;
        camera.angular_velocity.x = 0;
        camera.angular_velocity.y = 0;
        camera.angular_velocity.z = 0;
    }
    else
    {
        camera.velocity.x = f32(dx * double(0.01f));
        camera.velocity.y = f32(double(dy) * double(0.01f));
        camera.velocity.z = f32(double(dz) * double(0.01f));
    }
    if (vehicle.mass_override == 1)
        return false;
    vehicle.mass_override = 1;
    state.saved_controller_mode = camera.mode;
    camera.mode = 4;
    return true;
}
void detach_controller(player::Controller &controller, State &state, VehicleState &vehicle)
{
    if (vehicle.mass_override == 0)
        return;
    vehicle.mass_override = 0;
    controller.camera.mode = state.saved_controller_mode;
}
void check_direction(VehicleRaceState &state, Vec3 forward, std::int32_t mode)
{
    if (mode == 3)
        return;
    const auto &ref = state.respawn_heading;
    const double along = (double(ref.z) * forward.z + double(ref.y) * forward.y) + double(ref.x) * forward.x;
    if (along < double(-0.7070000171661377f))
    {
        ++state.opposing_direction_ticks;
        if (state.opposing_direction_ticks <= 600)
            return;
        state.respawn_requested = 1;
    }
    state.opposing_direction_ticks = 0;
}
void process_material(VehicleState &vehicle, VehicleRaceState &race, State &state, player::Controls &controls,
                      player::VehicleId id, Material material, float alignment, std::int32_t mode,
                      InvertedContact &inverted)
{
    const auto type = material.flags & 15;
    bool unsafe = false;
    if (type >= 1 && type <= 4)
    {
        if (auto index = player::find_controller(controls, id))
        {
            controls.current = *index;
            detach_controller(controls.controllers[*index], state, vehicle);
        }
        else
            vehicle.mass_override = 0;
        player::restore_primary(controls);
    }
    else if (type == 10 && vehicle.special_mode == 0 && vehicle.contact_class == 0)
    {
        state.special_ticks = 0;
        state.saved_2278 = std::bit_cast<std::uint32_t>(race.control_enabled);
        vehicle.special_mode = 1;
        state.special_accumulator.fill(0);
        state.saved_24a4 = race.spline_index;
    }
    if (material.flags & 0x40)
    {
        if (auto index = player::find_controller(controls, id))
        {
            controls.current = *index;
            if (attach_controller(controls.controllers[*index], state, vehicle))
                inverted.apply(vehicle, controls, id);
        }
        else
            vehicle.mass_override = 1;
        player::restore_primary(controls);
        if (vehicle.speed < 8.333333015441895f)
            unsafe = true;
    }
    else
        vehicle.stunt.inversion_latch = 0;
    if (material.flags & 0x80)
    {
        unsafe = true;
        reset_stunt_state(vehicle, vehicle);
    }
    if ((material.flags & 0x20) && vehicle.speed < 1.6666666269302368f)
        unsafe = true;
    check_direction(race, {vehicle.basis_z.x, vehicle.basis_z.y, vehicle.basis_z.z}, mode);
    if (!unsafe && alignment > 0.7071068286895752f)
    {
        race.unsafe_contact_ticks = 0;
        return;
    }
    ++race.unsafe_contact_ticks;
    if (race.unsafe_contact_ticks >= 180)
    {
        race.respawn_requested = 1;
        race.unsafe_contact_ticks = 0;
    }
}
} // namespace sgn::retail::contact::surface
