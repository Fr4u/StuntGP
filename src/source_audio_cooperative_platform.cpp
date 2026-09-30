#include "sgn/source_audio_cooperative.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioPrimaryBufferBoundary set_platform_source_audio_cooperative_462165(
    const SourceAudioCooperativeLevelBoundary &reached, RaceSessionPlatform &platform, InputWindowHandles handles)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    // window_input already validates handles against this world's acquisition
    // owner; it does not require the later4249B0 source input initialization.
    (void)platform.window_input(handles.instance, handles.window);
    auto &device = platform.source_audio_owner();
    return set_source_audio_cooperative_462165(reached, world.audio_initialization.device,
                                               world.audio_initialization.primary, handles.window, device);
}
} // namespace sgn::retail
