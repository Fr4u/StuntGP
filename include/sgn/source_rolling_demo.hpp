#pragma once
#include "sgn/source_boot_config.hpp"
namespace sgn::retail
{
// Full42CFA0 caller and token loop. Uses the existing platform script bank
// and the sole App-owned6129A4 byte region/612BA4 pointer table.
void load_source_rolling_demo_42cfa0(RaceSessionPlatform &, SourceBootSearchBoundary);
} // namespace sgn::retail
