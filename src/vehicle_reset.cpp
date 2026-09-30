#include "sgn/vehicle_reset.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>
#include <string>

namespace sgn::retail
{
namespace
{
ResetVehicle &at(VehicleResetWorld &world, unsigned index)
{
    if (index >= 6)
        throw std::out_of_range("vehicle reset slot");
    return world.vehicles[index];
}
std::uint32_t ftol(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw std::domain_error("respawn index outside source conversion range");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
void notice(VehicleResetWorld &world, ResetVehicle &car, std::string_view suffix, bool newline)
{
    if (car.id >= world.parameters.vehicle_names.size())
        throw std::out_of_range("reset notice vehicle identity");
    std::string text(world.parameters.vehicle_names[car.id]);
    text += ' ';
    text += suffix;
    if (newline)
        text += '\n';
    if (text.size() >= 64)
        throw std::domain_error("reset notice exceeds original 64-byte buffer");
    contact::player::post_notice(world.controls, world.parameters.stage, world.parameters.tick, 180, text, car.id, 1,
                                 0);
}
} // namespace
void consume_respawn_energy(ResetVehicle &car, const VehicleResetParameters &p)
{
    if (p.stage == 3)
        return;
    auto &s = car.physics;
    s.energy = vehicle_f32(double(s.energy) - (s.upgrade_active == 1 ? p.upgraded_energy_cost : p.normal_energy_cost));
    if (car.contact.impact_count.value < 0)
        car.contact.impact_count.value = 0;
}
bool exhausted_and_still(const VehicleState &s)
{
    return s.energy <= 0 && std::abs(s.speed) < std::bit_cast<float>(std::uint32_t{0x3fd55555});
}
void retire_current_vehicle(VehicleResetWorld &world)
{
    auto &car = at(world, world.current);
    if (car.physics.energy_exempt != 0 || car.race.stopped != 0)
        return;
    car.race.stopped = 1;
    car.race.finish_state = 0;
    disable_vehicle_control(car.race, car.physics);
    if (world.parameters.mode == 2)
        notice(world, car, world.parameters.retired_text, true);
}
void check_vehicle_retirement(VehicleResetWorld &world)
{
    if (world.parameters.mode == 3 && exhausted_and_still(at(world, world.current).physics))
        retire_current_vehicle(world);
}
void clear_special_globals(VehicleResetGlobals &globals) noexcept
{
    globals.special_timer = 0;
    globals.special_value = 0;
}
void prepare_special_respawn(VehicleResetWorld &world, unsigned target)
{
    auto &car = at(world, target);
    const auto &p = world.parameters;
    if (car.contact.surface.saved_24a4 == 0)
        return;
    if (!p.spacing_word)
        throw FormatError("special respawn requires original descriptor-adjacent spacing word");
    const double spacing = double(p.course_length) / double(*p.spacing_word);
    const auto backwards = ftol(5000.0f / spacing);
    car.race.spline_index = car.contact.surface.saved_24a4 - backwards;
    if (world.globals.transition_active == 1)
    {
        // 00447AA0 receives controller[0]+2F0. Its +10/+18 are the same
        // persistent controller fields at +300/+308, not a second UI object.
        auto &transition = world.controls.controllers[0].respawn_transition;
        transition.state = 1;
        transition.phase = 0;
        world.globals.transition_active = 0;
    }
}
void end_vehicle_special_mode(VehicleResetWorld &world, unsigned target)
{
    auto &car = at(world, target);
    car.race.timing26a8 = 1;
    const auto original_current = world.current;
    struct Restore
    {
        unsigned &value;
        unsigned previous;
        ~Restore()
        {
            value = previous;
        }
    } restore{world.current, original_current};
    world.current = target;
    if (car.physics.special_mode != 1)
        return;
    car.contact.surface.saved_24a4 = 0;
    car.physics.special_mode = 0;
    car.race.control_enabled = std::bit_cast<std::int32_t>(car.contact.surface.saved_2278);
    const auto found = contact::player::find_controller(world.controls, car.id);
    if (found)
    {
        world.controls.current = *found;
        auto &camera = world.controls.controllers[*found].camera;
        if (camera.mode != car.race.saved_camera_mode)
        {
            CameraEffectTargets targets;
            for (unsigned j = 0; j < 6; ++j)
            {
                targets.vehicles[j] = &world.vehicles[j].physics;
                targets.active[j] = static_cast<std::uint32_t>(world.vehicles[j].race.active);
            }
            camera_select_mode(camera, car.race.saved_camera_mode, camera_vehicle_reset_adapter(targets));
        }
    }
    contact::player::restore_primary(world.controls);
    notice(world, car, world.parameters.special_end_text, true);
    clear_special_globals(world.globals);
}
bool reset_vehicle(VehicleResetWorld &world, unsigned target)
{
    auto &car = at(world, target);
    auto &s = car.physics;
    auto &race = car.race;
    consume_respawn_energy(car, world.parameters);
    check_vehicle_retirement(world);
    if (race.stopped == 1)
        return false;
    reset_vehicle_motion(s);
    race.respawn_requested = 0;
    reset_camera_dependent_effects(s);
    prepare_special_respawn(world, target);
    end_vehicle_special_mode(world, target);
    reset_stunt_score_hold(s);
    reset_stunt_state(s, at(world, world.current).physics);
    s.gear = 1;
    const auto selected =
        select_respawn(world.parameters.spline, std::bit_cast<std::int32_t>(race.spline_index), world.parameters.mask);
    race.spline_index = std::bit_cast<std::uint32_t>(selected.persistent_segment);
    race.route_indices = selected.route_indices;
    race.respawn_position = selected.position;
    // 004127E0's fourth heading lane is unwritten stack data. Retain the
    // previous opaque word; it is not a direction component.
    race.respawn_heading.x = selected.heading.x;
    race.respawn_heading.y = selected.heading.y;
    race.respawn_heading.z = selected.heading.z;
    const auto p = race.respawn_position;
    const contact::Vector4 position{p.x, p.y, p.z, std::bit_cast<std::uint32_t>(p.w)};
    contact::Pose pose{};
    contact::response::place_rig(position, pose, car.contact.body, car.contact.rig);
    s.position = p;
    s.previous_position = p;
    const auto gravity = world.parameters.unit_gravity;
    const VehicleVector up{-gravity.x, -gravity.y, -gravity.z, gravity.w};
    auto right = vehicle_normalize(vehicle_cross(race.respawn_heading, up));
    // Second original unwritten local: first orientation-column W.
    right.w = s.orientation[3];
    s.orientation = vehicle_basis(right, up, race.respawn_heading);
    car.contact.body.orientation = s.orientation;
    if (world.parameters.selector == 2)
        race.collision_hold = 300;
    race.pending_reset = 0;
    race.input_hold = 30;
    if (const auto found = contact::player::find_controller(world.controls, car.id))
    {
        world.controls.current = *found;
        world.controls.controllers[*found].camera.collision_bypass_ticks = 300;
    }
    contact::player::restore_primary(world.controls);
    return true;
}
} // namespace sgn::retail
