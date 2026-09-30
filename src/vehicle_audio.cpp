#include "sgn/vehicle_audio.hpp"
#include "sgn/vehicle_audio_math.hpp"
#include "sgn/vehicle_audio_tables.hpp"
#include <algorithm>
#include <cmath>

namespace sgn::retail
{
namespace
{
using namespace audio_data;
using audio_detail::integer;
using audio_detail::signed_word;
Vec3 position(const VehicleAudioBindings &b)
{
    const auto &p = b.car.physics.position;
    return {p.x, p.y, p.z};
}
void loop(VehicleAudioBindings b, std::uint32_t &handle, std::uint32_t sample, std::uint32_t frequency,
          std::uint32_t volume, bool velocity = false)
{
    const auto *v = velocity ? &b.car.physics.velocity : nullptr;
    if (handle == 0)
        audio_start_positioned(b.driver, sample, 1, frequency, volume, position(b), v, 0, &handle);
    else
        audio_update_positioned(b.driver, handle, frequency, volume, position(b), v);
}
void event(VehicleAudioBindings b, std::uint32_t sample, std::uint32_t frequency, std::uint32_t volume,
           std::uint32_t *handle = nullptr)
{
    audio_start_positioned(b.driver, sample, 0, frequency, volume, position(b), nullptr, 0, handle);
}
std::uint32_t event_volume(double value)
{
    return static_cast<std::uint32_t>(std::clamp(signed_word(integer(value)), 0, 192)) + 32;
}
std::uint32_t surface_type(const VehicleAudioBindings &b, unsigned wheel)
{
    return std::bit_cast<std::uint32_t>(b.car.contact.wheels[wheel].type);
}
} // namespace
void vehicle_audio_grip(VehicleAudioBindings b)
{
    auto &s = b.car.physics;
    auto &h = b.car.audio.grip_handle;
    if (s.contact_class == 1 || std::abs(double(s.speed)) < c0046e264)
    {
        audio_stop_handle(b.driver, h);
        return;
    }
    double sum = 0;
    float pitch = 0;
    std::uint32_t changed = 0, type = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (s.wheels[i].grip_usage > c0046e31c)
        {
            changed |= std::bit_cast<std::uint32_t>(b.car.contact.wheels[i].changed);
            type = std::max(type, surface_type(b, i));
            const double value = double(s.wheels[i].grip_usage) * c0046e5fc;
            if (value > pitch)
                pitch = vehicle_f32(value);
            sum += double(s.wheels[i].grip_usage) * c0046e1f8 + c0046e454;
        }
    auto volume = signed_word(integer(sum * c0046e5f8));
    if (pitch > c0046e5f4)
        pitch = c0046e5f4;
    pitch = vehicle_f32(double(pitch) + c0046e5f0);
    if (volume < 1 || changed != 0)
    {
        audio_stop_handle(b.driver, h);
        return;
    }
    volume = std::min(volume, 255);
    // Table is read only when starting, as in the original update branch.
    loop(b, h, h ? 0 : grip.at(type), integer(pitch), static_cast<std::uint32_t>(volume));
}
void vehicle_audio_rolling(VehicleAudioBindings b)
{
    const auto &s = b.car.physics;
    auto &h = b.car.audio.rolling_handle;
    double sum = 0;
    std::uint32_t changed = 0, type = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (s.wheels[i].load > 0)
        {
            const auto &w = s.wheels[i];
            double value = (double(w.speed) * w.speed) * c0046e258;
            value = value < 1 ? value + value * c0046e5c0 : value + c0046e5c0;
            value *= double(w.load) / w.reference_load;
            changed |= std::bit_cast<std::uint32_t>(b.car.contact.wheels[i].changed);
            type = std::max(type, surface_type(b, i));
            sum += std::min(value, double(c0046e1e4));
        }
    if (sum < 1)
    {
        audio_stop_handle(b.driver, h);
        return;
    }
    const float volume = vehicle_f32(sum * c0046e60c);
    const float frequency = vehicle_f32(std::min(std::abs(double(s.speed)) * c0046e608, 1.0) * c0046e604 + c0046e600);
    if (changed)
        audio_stop_handle(b.driver, h);
    loop(b, h, h ? 0 : rolling.at(type), integer(frequency), integer(volume));
}
void vehicle_audio_landing(VehicleAudioBindings b)
{
    auto &c = b.car.contact;
    auto &a = b.car.audio;
    if (c.shock < c0046e20c)
    {
        a.previous_wheel_touched = std::bit_cast<std::uint32_t>(c.wheel_touched);
        c.wheel_touched = 0;
        return;
    }
    if (c.wheel_touched != 0 && a.previous_wheel_touched == 0)
    {
        event(b, 28, 0, event_volume(double(b.car.physics.average_load) * c0046e610));
        a.previous_wheel_touched = std::bit_cast<std::uint32_t>(c.wheel_touched);
    }
}
void vehicle_audio_scrape(VehicleAudioBindings b)
{
    auto &c = b.car.contact;
    auto &a = b.car.audio;
    const auto &s = b.car.physics;
    if (c.shock_hold == 0)
    {
        audio_stop_handle(b.driver, a.scrape_handle);
        c.shock_hold = 0;
        a.impact_handle = 0;
        return;
    }
    const float shock = c.shock;
    c.shock = 0;
    c.shock_hold = signed_word(std::bit_cast<std::uint32_t>(c.shock_hold) - 1u);
    if (s.speed > c0046e3cc && std::bit_cast<std::uint32_t>(c.shock_hold) > 0)
    {
        const double amplitude = std::min(std::abs(double(shock)), 1.0) * c0046e3ec;
        const float speed = vehicle_f32(std::abs(double(s.speed)));
        const float volume = vehicle_f32(std::min(double(speed) * c0046e620, 1.0) * amplitude);
        const double frequency = std::min(double(speed) * c0046e530, 1.0) * c0046e61c + c0046e618;
        // 41B3C0 reads the sample selector on both start and update branches.
        const auto sample = scrape.at(surface_type(b, 0));
        loop(b, a.scrape_handle, sample, integer(frequency), integer(volume));
    }
    else if (s.speed < c0046e3cc)
        audio_stop_handle(b.driver, a.scrape_handle);
    if (shock >= c0046e20c && a.impact_handle == 0)
    {
        const double amplitude = std::min(double(shock) * c0046e1f8 + c0046e570, double(c0046e1e4));
        const auto sample = impact[b.frontier.rotating_impact & 3];
        ++b.frontier.rotating_impact;
        event(b, sample, 0, integer((amplitude * c0046e1e4) * c0046e614), &a.impact_handle);
    }
}
void vehicle_audio_collision(VehicleAudioBindings b)
{
    auto &c = b.car.contact;
    if (c.effect_2368)
        --c.effect_2368;
    vehicle_audio_scrape(b);
    const float shock = c.pair_shock;
    c.pair_shock = 0;
    if (shock < c0046e20c)
        return;
    const double volume =
        std::min(((double(shock) * c0046e20c) * c0046e1e4) * c0046e614 + c0046e49c, double(c0046e1e4));
    if (c.effect_2368)
        return;
    const auto sample = pair_impact[b.frontier.rotating_impact & 3];
    ++b.frontier.rotating_impact;
    event(b, sample, 0, integer(volume));
    c.effect_2368 = 4;
}
void vehicle_audio_boost(VehicleAudioBindings b)
{
    auto &a = b.car.audio;
    const auto &s = b.car.physics;
    const auto &p = b.car.parameters;
    if (s.turbo_rpm == 0)
    {
        audio_stop_handle(b.driver, a.boost_handle);
        return;
    }
    const auto frequency = integer((double(s.turbo_rpm) * c0046e290) / p.turbo_max + c0046e628);
    const auto volume = integer((double(s.turbo_rpm) * c0046e624) / p.turbo_max + c0046e470);
    loop(b, a.boost_handle, a.boost_handle ? 0 : boost.at(static_cast<std::uint32_t>(b.car.load.sounds.engine_boost)),
         frequency, volume);
}
void vehicle_audio_surface_event(VehicleAudioBindings b)
{
    auto &a = b.car.audio;
    if (a.effect_hold)
    {
        --a.effect_hold;
        a.effect_pending = 0;
        return;
    }
    if (!a.effect_pending)
        return;
    a.effect_hold = 120;
    a.effect_pending = 0;
    const auto volume = event_volume(double(a.effect_strength) * c0046e1fc);
    event(b, effect.at(surface_type(b, 0)), 0, volume);
}
void vehicle_audio_events(VehicleAudioBindings b)
{
    auto &s = b.car.physics;
    auto &a = b.car.audio;
    if (s.launch_event)
    {
        event(b, 23, integer(double(s.launch_event_strength) * c0046e628 + c0046e628),
              integer(double(s.launch_event_strength) * c0046e1e4));
        s.launch_event = 0;
    }
    if (s.input.auxiliary)
        loop(b, a.horn_handle, a.horn_handle ? 0 : horn.at(static_cast<std::uint32_t>(b.car.load.sounds.horn)), 0, 224,
             true);
    else if (a.horn_handle)
        audio_stop_handle(b.driver, a.horn_handle);
    const auto controller = contact::player::find_controller(b.controls, b.car.id);
    if (controller)
    {
        b.controls.current = *controller;
        if (s.gear_changed == 1)
        {
            s.gear_changed = 0;
            const auto &listener = b.controls.controllers[*controller].camera.position;
            const auto p = position(b);
            const double x = double(listener.x) - p.x, y = double(listener.y) - p.y, z = double(listener.z) - p.z;
            double gain = (double(c0046e314) - std::sqrt((z * z + y * y) + x * x)) * c0046e62c;
            if (gain > 0)
            {
                gain = std::min(gain + c0046e230, 1.0);
                const std::uint32_t gear =
                    s.gear < 0 ? 0u - std::bit_cast<std::uint32_t>(s.gear) : std::bit_cast<std::uint32_t>(s.gear);
                const auto volume = integer((gain * gain) * double(gear_volume.at(gear)));
                audio_start(b.driver, 33, 0, 0, volume, 0, 1, nullptr);
            }
        }
    }
    contact::player::restore_primary(b.controls);
}
void vehicle_audio_motion(VehicleAudioBindings b)
{
    if (!contact::player::find_controller(b.controls, b.car.id))
        return;
    const auto &s = b.car.physics;
    auto &h = b.car.audio.motion_handle;
    if (s.speed < c0046e2b8)
    {
        audio_stop_handle(b.driver, h);
        return;
    }
    double sum = 0;
    for (const auto &w : s.wheels)
        sum += w.speed;
    const double gain = std::min((std::abs(sum) * c0046e24c) * c0046e294, 1.0);
    const float frequency = vehicle_f32(gain * c0046e630 + c0046e4dc);
    auto &shared = b.globals.motion_gain;
    if (s.throttle > c0046e2d4)
    {
        if (shared > c0046e1fc)
            shared = vehicle_f32(double(shared) - c0046e27c);
    }
    else if (shared < 1)
        shared = vehicle_f32(double(shared) + c0046e3d4);
    loop(b, h, 30, integer(frequency), integer((gain * shared) * c0046e46c));
}
void vehicle_audio_engine(VehicleAudioBindings b)
{
    const auto &s = b.car.physics;
    const auto &p = b.car.parameters;
    auto &a = b.car.audio;
    const double initial_frequency = std::abs(double(s.rpm)) * c0046e20c + c0046e628;
    float frequency = initial_frequency > c0046e63c ? c0046e63c : vehicle_f32(initial_frequency);
    float volume =
        vehicle_f32(((double(s.rpm) / p.rpm_max * c0046e484 + double(s.throttle) * c0046e390) + c0046e2d4) * c0046e1e4);
    if (b.globals.controller_gain == 100 && s.rpm < c0046e4e4)
    {
        volume = vehicle_f32(((double(s.rpm) * c0046e4e0) * c0046e1d8 + c0046e1d8) * volume);
        const double idle_volume =
            std::min(((double(c0046e4e4) - s.rpm) * c0046e4e0 + c0046e1d8) * c0046e2f0, double(c0046e1e4)) * c0046e638;
        const double raised = double(frequency) + c0046e630;
        frequency = raised > c0046e63c ? c0046e63c : vehicle_f32(raised);
        loop(b, a.idle_handle, a.idle_handle ? 0 : idle.at(static_cast<std::uint32_t>(b.car.load.sounds.engine_idle)),
             integer(frequency), integer(idle_volume), true);
    }
    else
        audio_stop_handle(b.driver, a.idle_handle);
    if (volume < c0046e634)
        volume = c0046e634;
    const double rev_volume = std::min((double(volume) * c0046e2f0) * c0046e614, double(c0046e2f0));
    loop(b, a.rev_handle, a.rev_handle ? 0 : rev.at(static_cast<std::uint32_t>(b.car.load.sounds.engine_rev)),
         integer(frequency), integer(rev_volume), true);
}
void vehicle_audio_steering(VehicleAudioBindings b)
{
    if (!contact::player::find_controller(b.controls, b.car.id))
        return;
    const auto &s = b.car.physics;
    auto &f = b.frontier;
    if (s.speed <= c0046e2b8 && std::abs(double(f.previous_steering) - s.steering) >= c0046e230)
    {
        if (!f.steering_handle)
            audio_start_positioned(b.driver, 32, 1, 0, 48, position(b), &s.velocity, 0, &f.steering_handle);
        else
            audio_update_positioned(b.driver, f.steering_handle, f.previous_steering < s.steering ? 18432u : 0u, 48,
                                    position(b), nullptr);
        f.steering_hold = 8;
        f.previous_steering = s.steering;
        return;
    }
    if (f.steering_hold)
    {
        if (f.steering_handle)
            audio_update_positioned(b.driver, f.steering_handle, 0, ((f.steering_hold * 3u) << 4) >> 3, position(b),
                                    nullptr);
        --f.steering_hold;
        f.previous_steering = s.steering;
        return;
    }
    if (f.steering_handle)
        audio_stop_handle(b.driver, f.steering_handle);
    f.previous_steering = s.steering;
}
void reset_vehicle_audio(VehicleAudioWorld &w)
{
    audio_stop_all(w.driver);
    for (auto &car : w.cars)
        if (car.race.active == 1)
        {
            for (auto &wheel : car.physics.wheels)
                wheel.grip_usage = 0;
            auto &a = car.audio;
            a.fields_233c.fill(0);
            a.effect_hold = 0;
            a.field_2350 = 0;
            a.effect_pending = 0;
            a.field_2358 = 0;
            a.effect_strength = 0;
            car.contact.shock = 0;
            car.contact.pair_shock = 0;
            car.contact.effect_2368 = 0;
            car.contact.shock_hold = 0;
            a.impact_handle = 0;
            a.previous_wheel_touched = 0;
            car.contact.wheel_touched = 0;
            a.grip_handle = 0;
            a.scrape_handle = 0;
            a.rolling_handle = 0;
            a.boost_handle = 0;
            a.idle_handle = 0;
            a.rev_handle = 0;
            car.physics.launch_event = 0;
            car.physics.launch_event_strength = 0;
            a.motion_handle = 0;
            car.physics.gear_changed = 0;
        }
}
void update_vehicle_audio(VehicleAudioWorld &w, unsigned current)
{
    if (current >= w.cars.size())
        throw FormatError("audio current vehicle outside six slots");
    if (&w.controls != &w.driver.controls)
        throw FormatError("audio listener and vehicle controller owners differ");
    if (w.pause_flag == 1)
    {
        audio_stop_all(w.driver);
        reset_vehicle_audio(w);
        return;
    }
    auto &car = w.cars[current];
    if (contact::player::find_controller(w.controls, car.id))
    {
        audio_set_range(w.driver.state, 15000);
        w.globals.controller_gain = 100;
    }
    else
        w.globals.controller_gain = 50;
    const VehicleAudioBindings b{car, w.driver, w.controls, w.globals, w.frontier};
    vehicle_audio_engine(b);
    vehicle_audio_boost(b);
    vehicle_audio_grip(b);
    vehicle_audio_rolling(b);
    vehicle_audio_motion(b);
    vehicle_audio_collision(b);
    vehicle_audio_steering(b);
    vehicle_audio_landing(b);
    vehicle_audio_surface_event(b);
    vehicle_audio_events(b);
    audio_set_range(w.driver.state, 7500);
}
} // namespace sgn::retail
