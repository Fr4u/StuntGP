#pragma once
#include "sgn/source_boot_config.hpp"

namespace sgn::retail
{
// Source42CC20 caller, writing the same Session level/clock/record owners.
// C-locale comparator and admitted Win64 x87 environment are required.
// Reached failures preserve prior writes, with no LevelSettings staging.
void configure_source_level_42cc20(RaceSessionPlatform &, SourceBootSearchBoundary);
} // namespace sgn::retail
