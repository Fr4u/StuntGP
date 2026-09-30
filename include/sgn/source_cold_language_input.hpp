#pragma once
#include "sgn/window_input_owner.hpp"
#include <cstdint>

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourceBootFilePlatform;
// Invocation observation only. The parent ignores the source0/1 return; this
// value is not a host readiness token or a qualification of the next child.
struct SourceColdLanguageInputObservation
{
    std::uint32_t ignored_input_return{};
    static constexpr std::uint32_t next_instruction = 0x442170;
    static constexpr std::uint32_t required_child = 0x442030;
};
// Reached44215C ->445F80,442161 ->4249B0,442166 store60DF9C=0.
// Requires the current sourcecold platform and its ALREADY attached real
// WindowInputOwner. No Session/preview/AssetStore preparation or fallback.
// Any normal input return continues to the store. Exceptions propagate with
// reached writes retained. Stops BEFORE the next442170 tail-call442030.
[[nodiscard]] SourceColdLanguageInputObservation initialize_platform_source_language_input_44215c(
    RaceSessionPlatform &, SourceBootFilePlatform &, InputWindowHandles);
} // namespace sgn::retail
