#pragma once

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourceBootFilePlatform;
// 443D70 ->443BD0 over the actual platform ScoreRules and boot file cache.
// Source pending==0 returns without reading files or clearing values/budget.
// Uses existing445B80/445AE0 token semantics and exact local4631F4; no
// ASCII-only replacement, second rules bank or AssetStore lookup.
// Full-read, NUL-free CSV <=1MiB with name<64/value<256 is the NEW host domain.
void ensure_source_score_configuration_443d70(RaceSessionPlatform &, SourceBootFilePlatform &);
} // namespace sgn::retail
