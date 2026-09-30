#include "sgn/source_race_level_script.hpp"
#include "sgn/assets.hpp"
#include "sgn/race_session_platform.hpp"

namespace sgn::retail
{
SourceRaceLevelScriptChildren::SourceRaceLevelScriptChildren(RaceSessionPlatform &platform,
                                                             SourceBootFilePlatform &files)
    : platform_(platform), clear_(platform), children_(files, clear_)
{
}

void SourceRaceLevelScriptChildren::apply_overrides_4453a0(RaceSessionPlatform &platform)
{
    platform_.require_owner_thread();
    if (&platform != &platform_)
        throw FormatError("race level-script children belong to another source platform");
    // Existing full 4453A0 preserves its own early return and exact dispatch:
    // 460BC0, optional445310, optional445360, mandatory444DB0, optional445120.
    // 445360 reaches the existing real442EF0/4199F0 file route;444DB0 reaches
    // the complete actual VehicleStorage clear, never a typed reset or no-op.
    apply_source_level_overrides_4453a0(platform, children_);
}

void load_source_race_level_script_42d070(RaceSessionPlatform &platform, SourceBootFilePlatform &files)
{
    SourceRaceLevelScriptChildren children(platform, files);
    // No catch/rollback/RAII script free:42D15E remains solely in the original
    // caller after4453A0 returns, retaining the source exception boundary.
    load_source_level_script_42d070(platform, files, children);
}
} // namespace sgn::retail
