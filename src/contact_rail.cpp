#include "sgn/contact_rail.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include "sgn/spline_resolution.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <string>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
std::uint32_t integer(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw std::domain_error("rail conversion outside source integer domain");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
ResetVehicle &current(RailWorld &w)
{
    if (w.reset.current >= 6)
        throw std::out_of_range("rail current vehicle");
    return w.reset.vehicles[w.reset.current];
}
CameraEffectTargets effects(RailWorld &w)
{
    CameraEffectTargets result;
    for (unsigned j = 0; j < 6; ++j)
    {
        result.vehicles[j] = &w.reset.vehicles[j].physics;
        result.active[j] = static_cast<std::uint32_t>(w.reset.vehicles[j].race.active);
    }
    return result;
}
bool tube_label(std::string_view value)
{
    constexpr std::string_view wanted = "trk_tubeglass";
    if (value.size() != wanted.size())
        return false;
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        auto c = value[i];
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c != wanted[i])
            return false;
    }
    return true;
}
} // namespace
void initialize_rail(RailState &state, const SplineSet &splines, std::span<const std::string> labels)
{
    state.spline = splines.find(2);
    state.tube_glass_texture.reset();
    for (std::size_t j = 0; j < labels.size(); ++j)
    {
        const auto separator = labels[j].find('\\');
        if (separator == std::string::npos)
            throw FormatError("rail texture label lacks source backslash separator");
        if (tube_label(std::string_view(labels[j]).substr(separator + 1)))
            state.tube_glass_texture = static_cast<std::uint32_t>(j);
    }
}
void begin_rail_notice(RailWorld &w)
{
    const auto &car = current(w);
    if (car.id >= w.reset.parameters.vehicle_names.size())
        throw std::out_of_range("rail notice vehicle identity");
    std::string text(w.reset.parameters.vehicle_names[car.id]);
    text += ' ';
    text += w.configuration.begin_text;
    text += '\n';
    if (text.size() >= 64)
        throw FormatError("rail notice exceeds source stack buffer");
    contact::player::post_notice(w.reset.controls, w.reset.parameters.stage, w.reset.parameters.tick, 180, text, car.id,
                                 1, 0);
    w.reset.globals.special_value = 1;
}
void update_rail_audio(RailWorld &w)
{
    const auto &car = current(w);
    if (car.physics.special_mode != 1)
        return;
    const auto &p = car.physics.position;
    Vec3 position{p.x, p.y, p.z};
    if (const auto found = contact::player::find_controller(w.reset.controls, car.id))
    {
        w.reset.controls.current = *found;
        const auto &camera = w.reset.controls.controllers[*found].camera.position;
        const float dx = vehicle_f32(double(p.x) - camera.x), dy = vehicle_f32(double(p.y) - camera.y);
        position = {vehicle_f32(double(dx) * 0.5), vehicle_f32(double(dy) * 0.5),
                    vehicle_f32((double(p.z) - camera.z) * 0.5)};
        position.x = vehicle_f32(double(position.x) + camera.x);
        position.y = vehicle_f32(double(position.y) + camera.y);
        position.z = vehicle_f32(double(position.z) + camera.z);
    }
    contact::player::restore_primary(w.reset.controls);
    auto &globals = w.reset.globals;
    if (globals.special_value != 0 && --globals.special_value == 0)
    {
        audio_start_positioned(w.audio, 39, 0, 0, 255, position, nullptr, 1, nullptr);
        globals.special_value = 30;
    }
    --globals.special_timer;
    if (as_signed(globals.special_timer) <= 0)
    {
        globals.special_timer = 30;
        const auto sound = (static_cast<std::uint32_t>(vehicle_random_integer(w.random, 1023)) & 3) + 40;
        audio_start_positioned(w.audio, sound, 0, 0, 170, position, nullptr, 1, nullptr);
    }
}
void update_rail_refuel(RailWorld &w)
{
    auto &car = current(w);
    auto &rail = w.vehicles[w.reset.current];
    const auto progress = as_signed(rail.progress);
    if (progress >= 40 && progress < 65)
    {
        auto &accum = car.contact.surface.special_accumulator;
        if (accum[2] == 0)
            begin_rail_notice(w);
        const auto emission = emit_rail_particles(w.particles, w.random, car.physics, rail);
        if (w.payload_audit)
        {
            auto &audit = *w.payload_audit;
            if (emission.primary)
                audit.particle_position[*emission.primary] = audit.vehicles[w.reset.current].position;
            if (emission.anchor)
            {
                audit.particle_position[*emission.anchor] = false;
                audit.particle_velocity[*emission.anchor] = true;
            }
        }
        update_rail_audio(w);
        ++accum[2];
        if (as_signed(accum[2]) % 3 != 0)
            return;
        const auto &parameters = w.parameters[w.reset.current];
        if (double(w.configuration.refuel_limit) * parameters.refuel_rate > as_signed(accum[0]) &&
            car.physics.energy < car.physics.energy_capacity)
        {
            const auto amount = integer(double(parameters.refuel_rate) * 10);
            accum[0] += amount;
            car.physics.energy = vehicle_f32(double(amount) + car.physics.energy);
        }
    }
    if (progress >= 65)
        clear_special_globals(w.reset.globals);
}
void update_rail(RailWorld &w, RailUpdateAudit *audit)
{
    if (audit)
        *audit = {};
    if (!w.state.spline)
    {
        w.splines.initialize_rail(w.state);
        return;
    } // R18: the track owner's 4192F0
    if (w.reset.parameters.mode == 1)
        return;
    auto &car = current(w);
    auto &s = car.physics;
    if (s.special_mode != 1)
        return;
    auto &rail = w.vehicles[w.reset.current];
    const auto &p = w.parameters[w.reset.current];
    disable_vehicle_control(car.race, s);
    s.turbo_active = 0;
    const double error = as_signed(rail.progress) > 75 ? 10.0 : double(w.configuration.target_speed) - s.speed;
    if (error > 0)
    {
        s.throttle = vehicle_f32(std::min(error * as_float(0x3dcccccd), 1.0));
        s.input.brake = 0;
        s.control_flags[2] = 1;
    }
    else if (error < as_float(0xc0555555))
    {
        if (error < as_float(0xc1855555))
        {
            s.velocity.x = vehicle_f32(double(s.velocity.x) * as_float(0x3f733333));
            s.velocity.y = vehicle_f32(double(s.velocity.y) * as_float(0x3f733333));
            s.velocity.z = vehicle_f32(double(s.velocity.z) * as_float(0x3f733333));
            s.velocity.x = vehicle_f32(double(w.configuration.gravity.x) + s.velocity.x);
            s.velocity.y = vehicle_f32(double(w.configuration.gravity.y) + s.velocity.y);
            s.velocity.z = vehicle_f32(double(w.configuration.gravity.z) + s.velocity.z);
            s.input.brake = 0;
        }
        else
            s.input.brake = static_cast<std::uint8_t>(integer(std::min(error * -16, 255.0)));
        s.throttle = 0;
        s.control_flags[2] = 0;
        digital_brake(p, s);
    }
    else
        s.throttle = vehicle_f32(double(s.throttle) * as_float(0x3f4ccccd));
    Vec3 ahead{vehicle_f32(double(s.basis_z.x) * 800), vehicle_f32(double(s.basis_z.y) * 800),
               vehicle_f32(double(s.basis_z.z) * 800)};
    ahead.x = vehicle_f32(double(ahead.x) + s.position.x);
    ahead.y = vehicle_f32(double(ahead.y) + s.position.y);
    ahead.z = vehicle_f32(double(ahead.z) + s.position.z);
    project_rail_position(resolve_spline(w.state.spline, 0x419310).value(), ahead, car.contact.surface.special_ticks,
                          rail); // R18 call-scoped view
    const auto gravity = w.reset.parameters.unit_gravity;
    const float dx = vehicle_f32(double(s.position.x) - rail.projected_position.x),
                dy = vehicle_f32(double(s.position.y) - rail.projected_position.y);
    const double dz = double(s.position.z) - rail.projected_position.z;
    const double projection = (double(gravity.z) * dz + double(gravity.y) * dy) + double(gravity.x) * dx;
    const float px = vehicle_f32(double(gravity.x) * projection), py = vehicle_f32(double(gravity.y) * projection);
    const Vec3 delta{vehicle_f32(double(px) + dx), vehicle_f32(double(py) + dy),
                     vehicle_f32(projection * gravity.z + dz)};
    const double steering =
        ((double(delta.z) * s.basis_x.z + double(delta.y) * s.basis_x.y) + double(delta.x) * s.basis_x.x) *
        as_float(0x3ba3d70a);
    if (std::abs(steering) > as_float(0x3dcccccd))
    {
        const float amount = vehicle_f32(steering);
        if (amount < 0)
            vehicle_steer_decrease(s.steering, p.steering_max, -amount);
        else
            vehicle_steer_increase(s.steering, p.steering_max, amount);
    }
    update_rail_refuel(w);
    const auto progress = as_signed(rail.progress);
    auto &globals = w.reset.globals;
    auto &transition = w.reset.controls.controllers[0].respawn_transition;
    if (progress < 15 && globals.transition_active == 0)
    {
        transition.state = 1;
        transition.phase = 0x42c80000;
        globals.transition_active = 1;
    }
    else if (progress > 70 && globals.transition_active == 1)
    {
        transition.state = 1;
        transition.phase = 0;
        globals.transition_active = 0;
    }
    if (const auto found = contact::player::find_controller(w.reset.controls, car.id))
    {
        w.reset.controls.current = *found;
        auto &camera = w.reset.controls.controllers[*found].camera;
        auto targets = effects(w);
        if (progress >= 75)
        {
            if (camera.mode != car.race.saved_camera_mode)
                camera_select_mode(camera, car.race.saved_camera_mode, camera_vehicle_reset_adapter(targets));
        }
        else if (progress >= 20)
        {
            const auto spline = resolve_spline(w.state.spline, 0x419310).value();
            const auto midpoint = spline.descriptor.point_count >> 1; // R18 call-scoped view
            const auto center = spline_point(spline, midpoint).value, next = spline_point(spline, midpoint + 2).value;
            const VehicleVector up{-gravity.x, -gravity.y, -gravity.z, gravity.w};
            auto direction =
                vehicle_normalize({vehicle_f32(double(center.x) - next.x), vehicle_f32(double(center.y) - next.y),
                                   vehicle_f32(double(center.z) - next.z), 0});
            auto side = vehicle_normalize(vehicle_cross(direction, up));
            side = {vehicle_f32(double(side.x) * -2500), vehicle_f32(double(side.y) * -2500),
                    vehicle_f32(double(side.z) * -2500), 0};
            direction = {vehicle_f32(double(direction.x) * -2000), vehicle_f32(double(direction.y) * -2000),
                         vehicle_f32(double(direction.z) * -2000), 0};
            const float uy = vehicle_f32(double(up.y) * 2000), uz = vehicle_f32(double(up.z) * 2000);
            const float cy = vehicle_f32(double(side.y) + center.y), cz = vehicle_f32(double(side.z) + center.z);
            const float y = vehicle_f32(double(cy) + uy), z = vehicle_f32(double(cz) + uz);
            camera.position = {vehicle_f32(double(direction.x) + ((double(side.x) + center.x) + double(up.x) * 2000)),
                               vehicle_f32(double(direction.y) + y), vehicle_f32(double(direction.z) + z), center.w};
            camera.twist = vehicle_f32(double(camera.twist) * as_float(0x3f666666));
            camera_select_mode(camera, 4, camera_vehicle_reset_adapter(targets));
            camera_set_lens(camera, as_float(0x4005c28f), w.shared_model_identity);
            camera_request_lens(camera, as_float(0x4005c28f));
            if (audit)
                audit->lens_controller = *found;
        }
    }
    contact::player::restore_primary(w.reset.controls);
    if (progress >= 99)
        end_vehicle_special_mode(w.reset, w.reset.current);
}
} // namespace sgn::retail
