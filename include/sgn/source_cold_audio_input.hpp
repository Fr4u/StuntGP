#pragma once
#include "sgn/source_audio_initialization_tail.hpp"
#include "sgn/window_input_owner.hpp"

namespace sgn::retail
{
// Invocation result only. result_site identifies the source observation; it
// may be a local branch with no HRESULT. This is not host readiness/admission.
struct SourceColdAudioResult
{
    bool source_return{};
    std::uint32_t result_site{};
    std::optional<AudioResult> native_result;
};
// Actual existing A1..A10 + shared tail on one retained platform. The caller
// reaches442152 after earlier cold steps and retains its attached HWND and
// prepared assets. The HWND is borrowed at A3, assets only after A7. There is
// no eager host preparation, retry, rollback, replacement owner or source
// teardown here. False and NEW admission exceptions retain their prefixes.
SourceColdAudioResult initialize_platform_source_audio_462090(RaceSessionPlatform &, InputWindowHandles);
// Exact bounded parent sequence442152 CALL462090 ->442157 CALL45EC30.
// Any normal Boolean return continues into physical input reset. An exception
// propagates without reset. Stop before44215C CALL445F80; this is not442120.
SourceColdAudioResult initialize_platform_source_audio_input_442152(RaceSessionPlatform &, InputWindowHandles);
} // namespace sgn::retail
