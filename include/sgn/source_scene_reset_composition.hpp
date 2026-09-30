#pragma once
#include "sgn/source_scene_reset_prefix.hpp"
#include "sgn/retail_scene_transition.hpp"

namespace sgn::retail
{
struct FrontendPerformanceCounter;

// Completes the admitted445FA0 prefix at4460CB:442100, then tail4192D0.
// A missing earlier owner is returned unchanged. A counter interruption keeps
// the prefix and first clock store; it never runs the later rail reset.
SourceSceneResetResult reset_source_scene_445fa0(RaceSessionPlatform &, FrontendPerformanceCounter &);

struct SourceSceneResetBoundary final
{
    SourceSceneResetResult result;
};

// Shared by the real Warm adapter and CPU receiving. True means445FA0 actually
// returned. False means this is another mandatory operation; no source work was
// performed by this helper. Never converts a counter interruption to success.
bool perform_source_scene_reset_445fa0(SceneOperation, RaceSessionPlatform &, FrontendPerformanceCounter &);

// One shared child dispatcher for Warm and the reached cold442030 binding.
// Results are NEW boundary observations, never source EAX/readiness values.
enum class SourceScenePrefixStatus
{
    completed,
    child_required,
    unhandled_operation
};
struct SourceScenePrefixResult
{
    SourceScenePrefixStatus status;
    std::uint32_t blocked_address{};
    std::uint64_t required_identity{};
};
SourceScenePrefixResult perform_source_scene_initialization_prefix(SceneOperation, RaceSessionPlatform &,
                                                                   FrontendPerformanceCounter &);

class FrontendState;
struct SourceColdSceneObservation
{
    std::uint32_t blocked_address;
    std::uint64_t required_identity;
};
// Reached442170 ->442030 only. Borrows the actual retained frontend/world;
// does not execute/repair predecessors, initialize the bank, or prepare owners.
// Calls the existing typed parent and shared children. Stops BEFORE42E6E0
// (442053), or an earlier unresolved audio boundary. Exceptions keep stores.
// This observation is not successful442030 completion or a resume token.
SourceColdSceneObservation initialize_source_cold_scene_442030(FrontendState &, FrontendPerformanceCounter &);

struct SourceBootFilePlatform;
struct InputWindowHandles;
struct SourceColdLanguageSceneObservation
{
    std::uint32_t ignored_input_return;
    SourceColdSceneObservation scene;
};
// Reached44215C through the REAL442170 tail, on the same retained frontend.
// Reuses both concrete implementations; a normal input0/1 ALWAYS enters442030.
// Exceptions retain earlier writes. No predecessor/backend/owner is created.
// The returned NEW observation is neither source success nor a resume token.
SourceColdLanguageSceneObservation initialize_source_cold_language_scene_44215c(FrontendState &,
                                                                                SourceBootFilePlatform &,
                                                                                InputWindowHandles,
                                                                                FrontendPerformanceCounter &);
} // namespace sgn::retail
