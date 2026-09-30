#pragma once
#include "sgn/source_boot_config.hpp"
#include "sgn/source_car_script.hpp"
#include <cstdint>
#include <string_view>

namespace sgn::retail
{

// Same representation as the existing global 589624: weak allocation
// identity, byte offset, source-null value. Copies do not retain payloads.
using SourceLevelScriptPointer = SourceBootSearchBoundary;

// Required calls, not optional hooks. No default/no-op implementations.
// Every binding operates on this exact platform, its App literals, bank,
// boot_config and scene owners. It must not create an alternate world.
// In particular, prefix-only children cannot satisfy these full-call contracts.
class SourceLevelScriptChildren
{
  public:
    virtual ~SourceLevelScriptChildren() = default;
    virtual void apply_overrides_4453a0(RaceSessionPlatform &) = 0;
};

// Complete caller control flow42D070..42D168, conditional on genuine full
// child bindings above. Does not assert scene readiness or activate a game.
// Host exceptions preserve reached effects; no synthetic rollback/release.
void load_source_level_script_42d070(RaceSessionPlatform &, SourceBootFilePlatform &, SourceLevelScriptChildren &);
} // namespace sgn::retail
