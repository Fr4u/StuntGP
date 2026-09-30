#pragma once
#include "sgn/source_audio_primary.hpp"
namespace sgn::retail::receiving {
struct PrimaryOsObservation {
    AudioResult result;
    bool reached_device_caps;
    bool native_success; //Requires exact zero AND nonnull retained primary.
};
//Main retains the A3 boundary from the same actual HWND/device chain. This
//function never constructs a window/device/world or repeats SetCooperativeLevel.
PrimaryOsObservation receive_primary_with_live_window(
    const SourceAudioPrimaryBufferBoundary&,RaceSessionPlatform&,InputWindowHandles);
}
