#include "sgn/retail_audio_maintenance.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/vehicle_audio_math.hpp"
#include "sgn/vehicle_audio_tables.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using audio_detail::signed_word;
std::int32_t music_pan(std::int32_t pan)
{
    if (pan < -255 || pan > 255)
        throw FormatError("music pan outside recovered table domain");
    const auto magnitude = static_cast<std::uint32_t>(pan < 0 ? -pan : pan);
    const std::int32_t value = audio_data::attenuation[128 - ((magnitude + 1) >> 1)];
    return pan < 0 ? -value : value;
}
std::int32_t volume_quotient(std::uint32_t product)
{
    // 45F844..45F857: signed magic division, including intermediate32-bit wrap.
    const auto wide = std::int64_t(signed_word(product)) * signed_word(0x81018203u);
    const auto high = static_cast<std::uint32_t>(wide >> 32);
    const auto shifted = signed_word(high + product) >> 15;
    return shifted + static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(shifted) >> 31);
}
} // namespace
void audio_recycle_duplicates(AudioDriver &d)
{
    if (d.state.duplicate_count == 0)
        return;
    for (unsigned i = 72; i < 104; ++i)
    {
        auto &sample = d.state.samples[i];
        const auto buffer = sample.buffer;
        const auto spatial = sample.spatial;
        if (!buffer.value)
            continue;
        std::uint32_t status{};
        if (d.device.status(buffer, status) != 0 || status != 0)
            continue;
        if (spatial.value)
            (void)d.device.release_spatial(spatial);
        (void)d.device.release(buffer);
        sample.buffer = {};
        sample.spatial = {};
        --d.state.duplicate_count;
    }
}
void audio_recount_playing(AudioDriver &d)
{
    d.state.playing_count = 0;
    const auto limit = d.state.duplicate_count + 72u;
    if (limit > 128)
        throw FormatError("audio recount bound outside recovered128slots");
    for (std::uint32_t i = 0; i < limit; ++i)
    {
        const auto buffer = d.state.samples[i].buffer;
        if (!buffer.value)
            continue;
        std::uint32_t status{};
        if (d.device.status(buffer, status) == 0 && (status & 1))
            ++d.state.playing_count;
    }
}
void audio_update_listener(AudioDriver &d, AudioListenerId listener, const contact::player::Controller &c,
                           const CameraPresentationState &presentation)
{
    if (!listener.value || c.active == 0)
        return;
    const auto &p = c.camera.position;
    const auto &v = c.camera.velocity;
    (void)d.device.listener_position(listener, {p.x, p.y, p.z}, 0);
    (void)d.device.listener_velocity(listener, {v.x, v.y, v.z}, 0);
    const auto &f = presentation.forward_axis;
    const auto &u = presentation.up_axis;
    (void)d.device.listener_orientation(listener, {f.x, f.y, f.z}, {-u.x, -u.y, -u.z}, 0);
}
void audio_apply_music_levels(AudioDriver &d, AudioMusicState &m, std::int32_t pan, std::uint32_t volume)
{
    if (d.level.output_a == 0)
        return;
    const auto buffer = m.buffer.borrowed();
    m.pan = pan;
    m.requested_volume = volume;
    if (!buffer.value)
        return;
    const auto mapped_pan = music_pan(pan);
    if (mapped_pan != m.cached_pan)
    {
        m.cached_pan = mapped_pan;
        if (d.device.pan(buffer, mapped_pan) != 0)
            return;
    }
    const auto q = volume_quotient((m.gain * m.requested_volume) * d.level.output_a);
    if (q == m.cached_volume)
        return;
    m.cached_volume = q;
    const auto value = audio_data::attenuation.at(static_cast<std::size_t>(q >> 1));
    (void)d.device.volume(m.buffer.borrowed(), value);
}
void audio_advance_music_gain(AudioDriver &d, AudioMusicState &m)
{
    if (d.level.output_a == 0)
        return;
    m.gain += m.gain_step;
    if (signed_word(m.gain) < 0 || signed_word(m.gain) > 255)
    {
        m.gain = signed_word(m.gain) < 0 ? 0u : 255u;
        m.gain_step = 0;
        m.cached_volume = -1;
        m.cached_pan = -1;
    }
    audio_apply_music_levels(d, m, m.pan, m.gain);
}
void audio_advance_sound_gain(LevelAudioState &level) noexcept
{
    auto &value = level.output_b;
    const auto target = level.output_b_copy;
    if (value == target)
        return;
    if (signed_word(value) < signed_word(target))
    {
        value += 4;
        if (signed_word(value) > signed_word(target))
            value = target;
    }
    else
    {
        value -= 4;
        if (signed_word(value) < signed_word(target))
            value = target;
    }
}
void audio_maintain(AudioMaintenance b)
{
    if (&b.level != &b.driver.level)
        throw FormatError("audio maintenance and driver level owners differ");
    if (!b.driver.state.initialized)
        return;
    audio_recycle_duplicates(b.driver);
    if (b.driver.state.spatial_mode == 1)
    {
        audio_update_listener(b.driver, b.state.listener, b.driver.controls.controllers[0], b.primary_presentation);
        audio_recount_playing(b.driver);
    }
    if (b.source_music)
        b.source_music->advance_gain(b.state.music);
    else
        audio_advance_music_gain(b.driver, b.state.music);
    audio_advance_sound_gain(b.level);
}
} // namespace sgn::retail
