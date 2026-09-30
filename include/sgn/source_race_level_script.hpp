#pragma once
#include "sgn/source_level_override_children_impl.hpp"
#include "sgn/source_vehicle_clear.hpp"

namespace sgn::retail
{
// Complete 42D157 ->4453A0 binding using the existing full source callers.
// Borrows one actual platform and the application's existing file backend;
// both must outlive this object. Contains only child adapters, no game data.
// Construction performs no vehicle clear, replay load or source-word store.
// Does not replace the separately constrained cold-menu loader.
class SourceRaceLevelScriptChildren final : public SourceLevelScriptChildren
{
    RaceSessionPlatform &platform_;
    SourceVehicleClear444dc9Impl clear_;
    SourceLevelOverrideChildrenImpl children_;

  public:
    SourceRaceLevelScriptChildren(RaceSessionPlatform &, SourceBootFilePlatform &);
    SourceRaceLevelScriptChildren(const SourceRaceLevelScriptChildren &) = delete;
    SourceRaceLevelScriptChildren &operator=(const SourceRaceLevelScriptChildren &) = delete;
    void apply_overrides_4453a0(RaceSessionPlatform &) override;
};

// Full existing 42D070 caller with the concrete 445360/replay/444DB0/clear
// dependencies above. Uses actual options.selection_phase: exact1 executes
// race overrides; every other value keeps the original early return.
// No scene, target, options, frontend or persistence owner is substituted.
// Read/format/allocation exceptions preserve reached source effects. This
// function does not complete 42E6E0, warm loading or race initialization.
void load_source_race_level_script_42d070(RaceSessionPlatform &, SourceBootFilePlatform &);
} // namespace sgn::retail
