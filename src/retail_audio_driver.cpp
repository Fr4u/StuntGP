#include "sgn/retail_audio_driver.hpp"
#include "sgn/vehicle_audio_math.hpp"
#include "sgn/vehicle_audio_tables.hpp"
#include "sgn/vehicle_math.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using audio_detail::integer;
using audio_detail::signed_word;
double distance_squared(CameraVector a, Vec3 b)
{
    const double x = double(a.x) - b.x, y = double(a.y) - b.y, z = double(a.z) - b.z;
    return (z * z + y * y) + x * x;
}
std::uint32_t free_duplicate(AudioDriverState &s)
{
    for (std::uint32_t i = 72; i < 104; ++i)
        if (s.samples[i].buffer.value == 0)
            return i;
    throw FormatError("audio duplicate count disagrees with occupied slots72..103");
}
std::int32_t volume_value(const AudioDriver &d, std::uint32_t volume, bool legacy_division = false)
{
    const std::uint32_t product = d.level.output_b * volume;
    const std::uint32_t index = legacy_division ? (product / 255u) >> 1 : product >> 9;
    return audio_data::attenuation.at(index);
}
std::int32_t pan_value(std::int32_t pan)
{
    if (pan < -255 || pan > 255)
        throw FormatError("audio pan outside recovered table domain");
    const auto magnitude = static_cast<std::uint32_t>(pan < 0 ? -pan : pan);
    const std::int32_t value = audio_data::attenuation[128 - ((magnitude + 1) >> 1)];
    return pan < 0 ? -value : value;
}
struct PositionValues
{
    VehicleVector direction, delta;
    float length;
};
PositionValues position_values(CameraVector listener, Vec3 position)
{
    VehicleVector delta{vehicle_f32(double(listener.x) - position.x), vehicle_f32(double(listener.y) - position.y),
                        vehicle_f32(double(listener.z) - position.z), 0};
    float length{};
    const auto direction = vehicle_normalize(delta, &length);
    return {direction, delta, length};
}
std::uint32_t attenuated_volume(const AudioDriver &d, std::uint32_t volume, float distance)
{
    const double gain = std::min((double(d.state.range) - distance) * d.state.inverse_range, 1.0);
    return integer(double(volume) * gain);
}
std::int32_t positional_pan(const AudioDriver &d, const CameraState &listener, const PositionValues &p, bool start)
{
    const double dot = (double(p.delta.z) * listener.side.z + double(p.delta.y) * listener.side.y) +
                       double(p.delta.x) * listener.side.x;
    // 4615F3..461601 divides; 46197F..46198B multiplies. Preserve this asymmetry.
    const double value = start ? dot * (double(audio_data::c0046e1e4) / d.state.inverse_range)
                               : (dot * d.state.inverse_range) * audio_data::c0046e1e4;
    return std::clamp(signed_word(integer(value)), -255, 255);
}
std::uint32_t doppler(std::uint32_t frequency, const CameraState &listener, const Vec3 *velocity,
                      VehicleVector direction)
{
    if (!velocity)
        return frequency;
    const double x = double(listener.velocity.x) - velocity->x, y = double(listener.velocity.y) - velocity->y,
                 z = double(listener.velocity.z) - velocity->z;
    const double dot = (z * direction.z + y * direction.y) + x * direction.x;
    // FST of the intermediate does not pop or round the live x87 value.
    const double factor = dot * audio_data::c0046ec6c + 1.0;
    return integer(factor * signed_word(frequency));
}
void start_spatial(AudioDriver &d, std::uint32_t sample, std::uint32_t flags, std::uint32_t frequency,
                   std::uint32_t volume, std::int32_t pan, Vec3 position, const Vec3 *velocity, bool positioned,
                   std::uint32_t force, std::uint32_t *handle)
{
    auto &s = d.state;
    if (!s.initialized || s.playing_count > 24)
        return;
    const auto source = s.samples.at(sample);
    auto buffer = source.buffer;
    auto spatial = source.spatial;
    if (!buffer.value || !spatial.value)
        return;
    if (positioned)
    {
        const auto &listener = d.controls.controllers[audio_listener(d, position)].camera;
        if (distance_squared(listener.position, position) > double(s.range_squared))
            return;
        if (handle)
            *handle = sample;
    }
    if (frequency == 0)
        frequency = source.frequency;
    if (force == 0 && !audio_duplicate_3d(d, buffer, spatial, sample, handle))
        return;
    if (positioned)
    {
        auto parameters = s.spatial_template;
        parameters.position = position;
        if (velocity)
            parameters.velocity = *velocity;
        parameters.maximum_distance = vehicle_f32(double(s.range));
        (void)d.device.spatial_parameters(spatial, parameters, 0);
    }
    else
        (void)d.device.spatial_mode(spatial, 2, 0);
    (void)d.device.frequency(buffer, frequency);
    (void)d.device.volume(buffer, volume_value(d, volume));
    if (!positioned)
        (void)d.device.pan(buffer, pan_value(pan));
    (void)d.device.play(buffer, 0, 0, flags == 1 ? 1u : 0u);
    ++s.playing_count;
}
} // namespace
AudioSpatialParameters audio_default_spatial_parameters()
{
    // 004897B8..004897F7; no guessed backend defaults.
    return {64, {0, 0, 0}, {0, 0, 0}, 360, 360, {0, 0, 1}, 0, 1300.0f, 7500.0f, 0};
}
void audio_set_range(AudioDriverState &s, std::uint32_t range)
{ // 00460D20..00460D57
    s.range = range;
    s.range_squared = range * range;
    s.inverse_range = vehicle_f32(1.0 / double(range));
}
void audio_stop_slot(AudioDriver &d, std::uint32_t slot)
{ // 00460D60..00460D89
    if (!d.state.initialized)
        return;
    const auto buffer = d.state.samples.at(slot).buffer;
    if (buffer.value)
        (void)d.device.stop(buffer);
    --d.state.playing_count;
}
void audio_stop_handle(AudioDriver &d, std::uint32_t &handle)
{ // 00460D90..00460DC4
    if (!d.state.initialized || handle == 0)
        return;
    const auto buffer = d.state.samples.at(handle).buffer;
    if (buffer.value)
        (void)d.device.stop(buffer);
    handle = 0;
    --d.state.playing_count;
}
void audio_stop_all(AudioDriver &d)
{ // 00460DD0..00460DE7
    for (std::uint32_t i = 0; i < 128; ++i)
        audio_stop_slot(d, i);
}
bool audio_duplicate_2d(AudioDriver &d, AudioBufferId &buffer, std::uint32_t sample, std::uint32_t *handle)
{
    std::uint32_t status{};
    if (d.device.status(buffer, status) != 0)
        return false;
    if ((status & 1) == 0)
        return true;
    if (signed_word(d.state.duplicate_count) >= 32)
        return false;
    const auto index = free_duplicate(d.state);
    auto &target = d.state.samples[index];
    if (d.device.duplicate(buffer, target.buffer) != 0)
        return false;
    buffer = target.buffer;
    ++d.state.duplicate_count;
    target.type = d.state.samples.at(sample).type;
    if (handle)
        *handle = index;
    return true;
}
bool audio_duplicate_3d(AudioDriver &d, AudioBufferId &buffer, AudioSpatialId &spatial, std::uint32_t sample,
                        std::uint32_t *handle)
{
    std::uint32_t status{};
    if (d.device.status(buffer, status) != 0)
        return false;
    if ((status & 1) == 0)
        return true;
    if (signed_word(d.state.duplicate_count) >= 32)
        return false;
    const auto index = free_duplicate(d.state);
    AudioBufferId new_buffer{};
    AudioSpatialId new_spatial{};
    if (d.device.duplicate(buffer, new_buffer) != 0)
        return false;
    if (d.device.query_spatial(new_buffer, new_spatial) != 0)
    {
        (void)d.device.release(new_buffer);
        return false;
    }
    auto &target = d.state.samples[index];
    buffer = new_buffer;
    target.buffer = new_buffer;
    spatial = new_spatial;
    target.spatial = new_spatial;
    ++d.state.duplicate_count;
    target.type = d.state.samples.at(sample).type;
    if (handle)
        *handle = index;
    return true;
}
unsigned audio_listener(const AudioDriver &d, Vec3 position)
{
    if (d.split_setting == 0)
        return 0;
    unsigned selected = 0;
    float best = std::bit_cast<float>(0x5268d4a5u);
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto &controller = d.controls.controllers[i];
        if (controller.active != 1)
            continue;
        const double distance = distance_squared(controller.camera.position, position);
        if (distance <= double(best))
        {
            best = vehicle_f32(distance);
            selected = i;
        }
    }
    return selected;
}
void audio_start(AudioDriver &d, std::uint32_t sample, std::uint32_t flags, std::uint32_t frequency,
                 std::uint32_t volume, std::int32_t pan, std::uint32_t force, std::uint32_t *handle)
{
    if (d.request_observer)
        d.request_observer->observe(sample, flags, frequency, volume, pan, force, handle);
    if (d.state.spatial_mode == 1 && d.state.samples.at(sample).type == 2)
    {
        start_spatial(d, sample, flags, frequency, volume, pan, {}, nullptr, false, force, handle);
        return;
    }
    if (!d.state.initialized)
        return;
    const auto source = d.state.samples.at(sample);
    auto buffer = source.buffer;
    if (!buffer.value)
        return;
    if (force == 0 && !audio_duplicate_2d(d, buffer, sample, handle))
        return;
    if (frequency == 0)
        frequency = source.frequency;
    if (d.device.frequency(buffer, frequency) != 0)
        return;
    if (d.device.volume(buffer, volume_value(d, volume, true)) != 0)
        return;
    if (d.device.pan(buffer, pan_value(pan)) != 0)
        return;
    (void)d.device.play(buffer, 0, 0, flags);
    ++d.state.playing_count;
}
void audio_start_positioned(AudioDriver &d, std::uint32_t sample, std::uint32_t flags, std::uint32_t frequency,
                            std::uint32_t volume, Vec3 position, const Vec3 *velocity, std::uint32_t force,
                            std::uint32_t *handle)
{
    if (d.state.spatial_mode == 1 && d.state.samples.at(sample).type == 2)
    {
        start_spatial(d, sample, flags, frequency, volume, 0, position, velocity, true, force, handle);
        return;
    }
    if (!d.state.initialized)
        return;
    const auto source = d.state.samples.at(sample);
    auto buffer = source.buffer;
    if (!buffer.value)
        return;
    const auto &listener = d.controls.controllers[audio_listener(d, position)].camera;
    const auto values = position_values(listener.position, position);
    if (double(values.length) > double(d.state.range))
        return;
    volume = attenuated_volume(d, volume, values.length);
    if (handle)
        *handle = sample;
    if (force == 0 && !audio_duplicate_2d(d, buffer, sample, handle))
        return;
    const auto pan = positional_pan(d, listener, values, true);
    if (frequency == 0)
        frequency = source.frequency;
    frequency = doppler(frequency, listener, velocity, values.direction);
    if (d.device.frequency(buffer, frequency) != 0)
        return;
    if (d.device.volume(buffer, volume_value(d, volume)) != 0)
        return;
    if (d.device.pan(buffer, pan_value(pan)) != 0)
        return;
    (void)d.device.play(buffer, 0, 0, flags);
    ++d.state.playing_count;
}
void audio_update_positioned(AudioDriver &d, std::uint32_t &handle, std::uint32_t frequency, std::uint32_t volume,
                             Vec3 position, const Vec3 *velocity)
{
    const bool spatial = d.state.spatial_mode == 1 && d.state.samples.at(handle).type == 2;
    if (!d.state.initialized)
        return;
    const auto source = d.state.samples.at(handle);
    if (!source.buffer.value || (spatial && !source.spatial.value))
        return;
    const auto &listener = d.controls.controllers[audio_listener(d, position)].camera;
    if (spatial)
    {
        if (distance_squared(listener.position, position) > double(d.state.range_squared))
        {
            audio_stop_handle(d, handle);
            return;
        }
        (void)d.device.spatial_position(source.spatial, position, 0);
        if (velocity)
            (void)d.device.spatial_velocity(source.spatial, *velocity, 0);
        if (frequency == 0)
            frequency = source.frequency;
        (void)d.device.frequency(source.buffer, frequency);
        (void)d.device.volume(source.buffer, volume_value(d, volume));
        return;
    }
    const auto values = position_values(listener.position, position);
    if (double(values.length) > double(d.state.range))
    {
        audio_stop_handle(d, handle);
        return;
    }
    volume = attenuated_volume(d, volume, values.length);
    const auto pan = positional_pan(d, listener, values, false);
    if (frequency == 0)
        frequency = source.frequency;
    frequency = doppler(frequency, listener, velocity, values.direction);
    if (d.device.frequency(source.buffer, frequency) != 0)
        return;
    if (d.device.volume(source.buffer, volume_value(d, volume)) != 0)
        return;
    (void)d.device.pan(source.buffer, pan_value(pan));
}
} // namespace sgn::retail
