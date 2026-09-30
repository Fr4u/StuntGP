#pragma once
#include "sgn/source_cold_time.hpp"
#include "sgn/source_cold_language_input.hpp"
#include "sgn/retail_scene_transition.hpp"

namespace sgn::retail
{
class FrontendState;
struct FrontendPerformanceCounter;
// Invocation-only borrows. No clocks, RNG, allocations, audio, fonts, input
// or scene state are created here. Scene services own their existing children.
struct SourceColdInitializationContext
{
    FrontendState &frontend;
    SourceColdTimePlatform &time;
    FrontendPerformanceCounter &counter;
    SourceBootFilePlatform &files;
    InputWindowHandles input_handles;
    SceneTransitionServices &scene;
};
// Full442120 caller order through its442170 tail into the existing442030
// implementation. Any missing audio/scene child must reject synchronously;
// exceptions propagate and preserve all previously reached writes.
void initialize_source_cold_442120(SourceColdInitializationContext &);
} // namespace sgn::retail
