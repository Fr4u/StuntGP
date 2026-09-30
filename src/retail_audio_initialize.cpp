#include "sgn/retail_audio_initialize.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
void publish_enumeration_guid(AudioEnumerationState &state, AudioListenerId &listener,
                              const std::optional<std::array<std::uint32_t, 4>> &guid)
{
    if (guid)
    {
        state.cursor_word = (*guid)[0];   // 462043, context+0, not a host pointer
        listener.value = (*guid)[1];      // 462048, context+4, same listener owner
        state.count = (*guid)[2];         // 462051, context+8
        state.adjacent_word = (*guid)[3]; // 462054, context+C
    }
}
bool copy_enumeration_description(AudioEnumerationState &state, std::string_view description)
{
    // SCASB stops at the first NUL. A string_view end represents its implicit
    // terminator; bytes after an explicit NUL are not part of the source name.
    const auto end = description.find('\0');
    if (end != std::string_view::npos)
        description = description.substr(0, end);
    const auto size = description.size();
    const auto retained = std::min(size, state.records.size());
    if (retained)
        std::copy_n(description.begin(), retained, state.records.begin());
    if (size >= state.records.size())
    {
        // The owner ends at1152 bytes. Preserve the supported write prefix and
        // reject the first write outside it, never damage a C++ neighbor. The
        // source copies whole DWORDs at462071, then residual bytes at46207D.
        const auto site = size >= state.records.size() + 3 ? 0x462071u : 0x46207du;
        throw SourceAudioEnumerationError(site, "462030 description exceeds supported source record extent");
    }
    state.records[size] = 0; // Copy includes NUL; the remaining tail is unchanged.
    return true;
}
struct SourceEnumerationReceiver final : AudioEnumerationReceiver
{
    AudioEnumerationState &state;
    AudioListenerId &listener;
    SourceEnumerationReceiver(AudioEnumerationState &s, AudioListenerId &l) : state(s), listener(l) {}
    bool visit(const std::optional<std::array<std::uint32_t, 4>> &guid, std::string_view description) override
    {
        return record_audio_enumeration(state, listener, guid, description);
    }
    bool visit_ansi(const std::optional<std::array<std::uint32_t, 4>> &guid, const char *description) override
    {
        return record_audio_enumeration_ansi(state, listener, guid, description);
    }
};
} // namespace
bool record_audio_enumeration(AudioEnumerationState &state, AudioListenerId &listener,
                              const std::optional<std::array<std::uint32_t, 4>> &guid, std::string_view description)
{
    publish_enumeration_guid(state, listener, guid);
    return copy_enumeration_description(state, description);
}
bool record_audio_enumeration_ansi(AudioEnumerationState &state, AudioListenerId &listener,
                                   const std::optional<std::array<std::uint32_t, 4>> &guid, const char *description)
{
    publish_enumeration_guid(state, listener, guid);
    if (!description)
        throw SourceAudioEnumerationError(0x462062u, "462062 null ANSI description");
    // The SDK supplies a readable NUL-terminated buffer. GUID writes have already
    // occurred before this strlen, matching the callback instruction order.
    return copy_enumeration_description(state, std::string_view(description, std::strlen(description)));
}
bool AudioEnumerationReceiver::visit_ansi(const std::optional<std::array<std::uint32_t, 4>> &guid,
                                          const char *description)
{
    if (!description)
        throw SourceAudioEnumerationError(0x462062u, "462062 null ANSI description");
    return visit(guid, std::string_view(description));
}
AudioResult enumerate_audio_devices_46210e(AudioEnumerationState &state, AudioListenerId &listener,
                                           AudioEnumerationPlatform &platform)
{
    SourceEnumerationReceiver receiver(state, listener);
    const auto result = platform.enumerate(receiver);
    if (result != 0)
        return result; // 462113/115 tests zero, not HRESULT sign.
    const auto difference = std::bit_cast<std::int32_t>(state.cursor_word - AudioEnumerationState::source_origin);
    state.count = std::bit_cast<std::uint32_t>(difference / 144); // 462140, signed truncation
    return result;
}
bool initialize_audio_prefix_462090(AudioInitializationState &state, AudioDriverState &driver,
                                    AudioMaintenanceState &maintenance, LevelAudioState &level)
{
    // Existing source descriptor projection. PE489D40+8, stride12;4620A3.
    state.descriptor_types[0] = 1;
    for (const auto &definition : audio_sample_definitions)
        state.descriptor_types[definition.slot] = definition.type;
    state.enumeration.records = {};                                       // 4620AF,1152-byte source extent
    const auto effects_input = level.input_b;                             // 4620B1, read before later stores
    state.enumeration.cursor_word = AudioEnumerationState::source_origin; // 4620BF
    state.enumeration.count = 0;                                          // 4620C9
    state.spatial_enabled = 1;                                            // 4620CF
    state.spatial_capability = 1;                                         // 4620D5
    driver.initialized = 0;
    state.device = {};
    state.primary = {}; // 4620DB..4620E7
    maintenance.listener = {};
    level.output_b = 256;      // 4620ED..4620F3
    return effects_input != 0; // 4620FD; all preceding mutations survive false
}
bool complete_audio_initialization_tail_462187(bool catalog_return, AudioDriverState &driver, AudioStreamState &stream,
                                               AudioMusicState &music, LevelAudioState &level,
                                               LevelOverrideOptions &options,
                                               const std::uint32_t &spatial_enabled) noexcept
{
    if (!catalog_return)
        return false;                             // 462187..46218D; no tail side effects.
    driver.initialized = 1;                       // 4621A2, before45FA60.
    initialize_audio_music(stream, music, level); // 4621A8; existing45FA60/45FAC0.
    update_level_audio(level, options);           // 4621AD; overwrites temporary output_a=0x66.
    driver.spatial_mode = spatial_enabled;        // 4621B2/4621B7: read after both children.
    return true;                                  // 4621BC..4621C0. No host readiness or parent continuation.
}
bool initialize_audio(AudioStartupWorld &world)
{
    if (&world.audio.level != &world.level)
        throw std::invalid_argument("audio initialization level owner mismatch");
    auto &state = world.state;
    auto &driver = world.audio.state;
    if (!initialize_audio_prefix_462090(state, driver, world.maintenance, world.level))
        return false;
    if (enumerate_audio_devices_46210e(state.enumeration, world.maintenance.listener, world.platform) != 0)
        return false;
    if (world.platform.create_default_device(state.device) != 0)
        return false;
    world.platform.cooperative_level(state.device, world.window, 3);
    if (!initialize_audio_primary(state.device, state.primary, world.platform, state.requested_rate,
                                  state.spatial_capability, state.spatial_enabled))
        return false;
    if (driver.spatial_mode == 0)
        state.spatial_enabled = 0;
    else if (state.spatial_enabled == 1 &&
             !initialize_audio_listener(state.primary, world.maintenance.listener, world.platform))
        return false;
    const bool loaded = load_audio_samples(driver, state.sample_fields, state.descriptor_types, state.device,
                                           world.platform, world.assets, state.spatial_enabled);
    return complete_audio_initialization_tail_462187(loaded, driver, world.stream, world.maintenance.music, world.level,
                                                     world.options, state.spatial_enabled);
}
void shutdown_audio(AudioStartupWorld &world)
{
    if (world.audio.state.initialized)
    {
        shutdown_audio_music(world.stream, world.maintenance.music, world.level, world.audio.device,
                             world.stream_platform);
        if (world.state.device.value)
        {
            world.platform.release_device(world.state.device);
            world.state.device = {};
        }
    }
}
void reconfigure_audio_for_split(AudioStartupWorld &world)
{
    auto &state = world.state;
    auto &mode = world.audio.state.spatial_mode;
    state.saved_spatial_mode = mode;
    const auto desired = world.audio.split_setting ? 0u : state.spatial_capability;
    const bool changed = mode != desired;
    mode = desired;
    if (changed)
    {
        shutdown_audio(world);
        initialize_audio(world);
    }
}
} // namespace sgn::retail
