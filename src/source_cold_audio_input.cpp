#include "sgn/source_cold_audio_input.hpp"
#include "sgn/source_input_cold_init.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceColdAudioResult initialize_platform_source_audio_input_442152(RaceSessionPlatform &platform,
                                                                    InputWindowHandles handles)
{
    platform.require_owner_thread();
    const auto audio = initialize_platform_source_audio_462090(platform, handles); // 442152.
    initialize_platform_source_physical_input_45ec30(platform); // 442157, unconditional after normal return.
    return audio;                                               // NEW observation only. Stop before44215C CALL445F80.
}
} // namespace sgn::retail
