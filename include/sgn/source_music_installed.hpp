#pragma once
#include "sgn/source_music_calls.hpp"

namespace sgn::retail
{
std::shared_ptr<SourceMusicTimerCapsReceiving> make_installed_music_caps_receiving();
std::shared_ptr<SourceMusicDriveTypeReceiving> make_installed_music_drive_receiving();
// Explicit one-time host setup before source music callers. Uses the existing
// App binding and platform root; it neither calls45FB50 nor creates a device.
void install_source_music_windows(std::shared_ptr<RaceSessionPlatform>);
} // namespace sgn::retail
