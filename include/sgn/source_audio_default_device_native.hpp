#pragma once
#include <windows.h>
#include <mmreg.h>
#include <dsound.h>
#include "sgn/source_audio_listener_reference_transfer.hpp"

namespace sgn
{
using DirectSoundCreateEntry = decltype(&::DirectSoundCreate);
// Value-copied at deferred construction. No global mutable production hook.
// The default is the actual installed import; alternate entries are receiving
// seams and must return NULL or a valid owned IDirectSound reference when writing
// an output, including on failure. Invalid pointer/SEH outputs are not admitted.
struct SourceAudioDefaultDeviceNativeApi
{
    DirectSoundCreateEntry create = &::DirectSoundCreate;
    // Null uses the ordinary SDK QueryInterface and its successful OUT ref.
    // A receiving entry transfers ownership explicitly through the scoped sink.
    // S_OK/non-null must match its accepted ref; erased/null outputs, failure
    // and exceptions retain an already transferred ref without rewriting OUT.
    SourceAudioListenerQueryReceivingEntry listener_query_receiving = nullptr;
};
} // namespace sgn
