#pragma once
#include "sgn/source_music_timer_entry_prefix.hpp"

namespace sgn::retail
{
#if defined(_WIN32)
// NEW installed WinMM bridge for the proposed one-call source prefix.
// Requires an explicitly prepared same-platform period owner; this wrapper
// neither prepares nor replaces it. The direct TIMECAPS output stays in actual
// state. Continues through the owned Begin and private root copy, stopping
// before45FE17 GetDriveTypeA. No source caller or music-buffer admission token.
SourceMusicTimerPrefixObservation receive_installed_source_music_timer_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform>);
#endif
} // namespace sgn::retail
