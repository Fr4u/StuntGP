#include "sgn/source_music_calls.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
void shutdown_source_audio_4621d0(RaceSessionPlatform &p)
{
    p.require_owner_thread();
    auto &state = p.state();
    if (state.audio_driver.initialized == 0)
        return;
    shutdown_source_music_45fab0(p);                       // 4621D9; exceptions preserve child prefix
    const auto device = state.audio_initialization.device; // 4621DE reload AFTER child
    if (device.value == 0)
        return;
    (void)p.source_audio_owner().release_device(device); // 4621EA, one retained COM reference
    state.audio_initialization.device = {};              // 4621ED only after ordinary Release return
    // Source leaves driver.initialized, primary, listener and sample globals
    // unchanged. No extra child cleanup or implicit reinitialization here.
}
} // namespace sgn::retail
