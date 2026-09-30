#pragma once
#include "sgn/source_audio_cooperative.hpp"

namespace sgn::retail::receiving {
//Host fixture owns the HWND, world/platform, successful A2 boundary and retained
//interface. This receiving does not create/select a window/device/world or call
//a display mode/fullscreen API. Caller performs final source/native cleanup.
//Nonzero actual cooperative HRESULT is returned with native_success=false;
//Main must record failure/inconclusive, never an actual-OS success PASS.
struct CooperativeOsObservation {AudioResult result;bool native_success;};
CooperativeOsObservation receive_cooperative_with_live_window(
    const SourceAudioCooperativeLevelBoundary&,RaceSessionPlatform&,InputWindowHandles);
}
