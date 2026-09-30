#include "sgn/source_menu_sample_native.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/platform_audio.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
bool duplicate_source_spatial_460e70(RaceSessionPlatform &platform, WindowsAudioDevice &native, AudioBufferId &buffer,
                                     AudioSpatialId &spatial, std::uint32_t sample, std::uint32_t *handle)
{
    auto &world = platform.state();
    auto &driver = world.audio_driver;
    const auto original_buffer = buffer; // 460E81 before GetStatus
    std::uint32_t status = 0;            // NEW transport hygiene; source consumes OUT only for HRESULT==0
    if (native.source_sample_status(original_buffer, status, 0x460e87) != 0)
        return false;
    if ((status & 1u) == 0)
        return true;
    if (std::bit_cast<std::int32_t>(driver.duplicate_count) >= 32)
        return false; // 460EA3 signedJL
    std::uint32_t index = 72;
    while (index < driver.samples.size() && driver.samples[index].buffer.value != 0)
        ++index;
    if (index == driver.samples.size())
        throw FormatError("460EC5 spatial duplicate scan exceeds actual source sample owner");
    auto &target = driver.samples[index]; // same source ESI across both native calls
    // 460EDC writes a local, not target.buffer. Both actual native references
    // remain in the existing source COM ledger until a reached Release/cleanup.
    AudioBufferId duplicate{};
    if (native.source_sample_duplicate(world.audio_initialization.device, original_buffer, duplicate, 0x460edc) != 0)
        return false;
    AudioSpatialId queried{};
    if (native.source_sample_query_spatial(duplicate, queried) != 0)
    {
        (void)native.source_sample_release(duplicate, 0x460f09);
        return false;
    }
    const auto previous_count = driver.duplicate_count; // 460F20 before either publication
    buffer = duplicate;                                 // 460F26, caller's local buffer
    target.buffer = duplicate;                          // 460F28
    spatial = queried;                                  // 460F2D, caller's local spatial
    target.spatial = queried;                           // 460F32
    const auto next_count = previous_count + 1u;        // 460F35, wrapping DWORD
    const auto type = driver.samples.at(sample).type;   // 460F36 AFTER both target handles
    driver.duplicate_count = next_count;                // 460F41
    target.type = type;                                 // 460F47
    if (handle)
        *handle = index; // 460F55, may alias an actual source state DWORD
    return true;
}

void start_source_spatial_461030(RaceSessionPlatform &platform, std::uint32_t sample, std::uint32_t flags,
                                 std::uint32_t frequency, std::uint32_t volume, std::int32_t pan, std::uint32_t force,
                                 std::uint32_t *handle)
{
    auto &world = platform.state();
    auto &driver = world.audio_driver;
    if (driver.initialized == 0)
        return; // 461036..461041
    if (driver.playing_count > 24u)
        return;                                     // 461047..46104E, unsignedJA
    auto buffer = driver.samples.at(sample).buffer; // 461059, call-local identity
    if (buffer.value == 0)
        return;
    auto spatial = driver.samples.at(sample).spatial; // 46106A, after buffer guard
    if (spatial.value == 0)
        return;
    auto &native = platform.source_audio_owner(); // borrow only; no host backend preparation
    if (force == 0 && !duplicate_source_spatial_460e70(platform, native, buffer, spatial, sample, handle))
        return;
    (void)native.source_sample_spatial_mode(spatial, 2, 0); // 4610A5; HRESULT ignored
    if (frequency == 0)
        frequency = driver.samples.at(sample).frequency; // 4610AF, AFTER SetMode callback
    (void)native.source_sample_frequency(buffer, frequency, 0x4610bc);
    const auto level = world.audio_level.output_b; // 4610BF, AFTER SetFrequency callback
    const auto literals = platform.source_application_literals();
    const std::uint32_t product = level * volume; // 4610C8, low DWORD IMUL
    const auto index = product >> 9;              // 4610CE: source spatial division512, not2D's division510
    const auto attenuation =
        std::bit_cast<std::int16_t>(literals.read_audio_level_word(0x4897f8u + (index << 1), 0x4610d1));
    (void)native.source_sample_volume(buffer, attenuation, 0x4610db);
    // 4610DE..4610FB: signed abs, INC, SAR1, SHL1 and NEG all wrap as DWORD.
    // The live attenuation owner is reread after the SetVolume callback.
    const auto sign = pan < 0 ? 0xffffffffu : 0u;
    const auto magnitude = ((std::bit_cast<std::uint32_t>(pan) ^ sign) - sign) + 1u;
    const auto shifted = (magnitude >> 1) | (magnitude & 0x80000000u);
    auto pan_bits = static_cast<std::uint32_t>(static_cast<std::int32_t>(
        std::bit_cast<std::int16_t>(literals.read_audio_level_word(0x4898f8u - (shifted << 1), 0x4610f6))));
    if (pan < 0)
        pan_bits = 0u - pan_bits;
    (void)native.source_sample_pan(buffer, std::bit_cast<std::int32_t>(pan_bits), 0x461104);
    const bool loop = flags == 1; // 46110A, exactly1; other flags become0 on this branch
    (void)native.source_sample_play(buffer, loop ? 1u : 0u, loop ? 0x461118u : 0x461137u);
    ++driver.playing_count; // 46111B..461121 or46113A, even nonzero Play HRESULT
}
} // namespace
SourceMenuSampleNative::SourceMenuSampleNative(const std::shared_ptr<RaceSessionPlatform> &p)
    : platform_(p), thread_(std::this_thread::get_id())
{
    if (!p)
        throw std::invalid_argument("source menu samples require a platform");
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw std::invalid_argument("source sample receiver requires source platform");
}
void SourceMenuSampleNative::start_461150(FrontendState &frontend, std::uint32_t sample, std::uint32_t flags,
                                          std::uint32_t frequency, std::uint32_t volume, std::int32_t pan,
                                          std::uint32_t force, std::uint32_t *handle)
{
    if (std::this_thread::get_id() != thread_)
        throw std::logic_error("source menu samples called from another thread");
    const auto p = platform_.lock();
    if (!p)
        throw std::logic_error("source menu sample platform expired");
    p->require_owner_thread();
    if (frontend.shared_platform().get() != p.get())
        throw std::logic_error("source menu samples belong to another frontend");
    auto &world = p->state();
    auto &driver = world.audio_driver;
    // 461154..461186 precedes the initialized gate. This source branch owns
    // its own gates and native call ordering, including ignored HRESULTs.
    if (driver.spatial_mode == 1 && driver.samples.at(sample).type == 2)
    {
        start_source_spatial_461030(*p, sample, flags, frequency, volume, pan, force, handle);
        return; // 46118B..461190, never falls through into2D
    }
    if (driver.initialized == 0)
        return;
    auto buffer = driver.samples.at(sample).buffer; // 4611A5 call-local identity
    if (buffer.value == 0)
        return;
    auto &native = p->source_audio_owner(); // no lazy construction or modern attachment
    if (force == 0)
    {
        std::uint32_t status = 0; // native output, never consumed on failure
        if (native.source_sample_status(buffer, status) != 0)
            return;
        if ((status & 1u) != 0)
        {
            if (std::bit_cast<std::int32_t>(driver.duplicate_count) >= 32)
                return; // 460FA6 signedJL
            // The source scans by32 until a null buffer, with no104 guard.
            // Stop at the end of the actual128-record owner rather than invent memory.
            std::uint32_t index = 72;
            while (index < driver.samples.size() && driver.samples[index].buffer.value != 0)
                ++index;
            if (index == driver.samples.size())
                throw FormatError("460FC1 duplicate scan exceeds source sample owner");
            auto &target = driver.samples[index];
            const auto hr = native.source_sample_duplicate(world.audio_initialization.device, buffer, target.buffer);
            if (hr != 0)
                return;                                         // output COM writes survive this return
            buffer = target.buffer;                             // 460FEA/460FF5
            const auto previous_count = driver.duplicate_count; // 460FEC before type read
            const auto type = driver.samples.at(sample).type;
            driver.duplicate_count = previous_count + 1u;
            target.type = type;
            if (handle)
                *handle = index;
        }
    }
    if (frequency == 0)
        frequency = driver.samples.at(sample).frequency; // reload after duplication
    if (native.source_sample_frequency(buffer, frequency) != 0)
        return;
    const auto literals = p->source_application_literals();
    const std::uint32_t product = world.audio_level.output_b * volume;
    const auto high = static_cast<std::uint32_t>((std::uint64_t{0x80808081u} * product) >> 32);
    const auto index = (high >> 7) >> 1;
    const auto attenuation =
        std::bit_cast<std::int16_t>(literals.read_audio_level_word(0x4897f8u + index * 2u, 0x46120a));
    if (native.source_sample_volume(buffer, attenuation) != 0)
        return;
    const auto sign = pan < 0 ? 0xffffffffu : 0u;
    auto magnitude = (std::bit_cast<std::uint32_t>(pan) ^ sign) - sign;
    magnitude += 1u;
    const auto shifted = (magnitude >> 1) | (magnitude & 0x80000000u);
    auto pan_bits = static_cast<std::uint32_t>(static_cast<std::int32_t>(
        std::bit_cast<std::int16_t>(literals.read_audio_level_word(0x4898f8u - (shifted << 1), 0x461233))));
    if (pan < 0)
        pan_bits = 0u - pan_bits;
    if (native.source_sample_pan(buffer, std::bit_cast<std::int32_t>(pan_bits)) != 0)
        return;
    (void)native.source_sample_play(buffer, flags); // 46124B branches converge; flags unchanged
    ++driver.playing_count;                         // 46125B executes even on nonzero Play return
}
} // namespace sgn::retail
