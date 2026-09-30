#pragma once
#include "sgn/source_script_path_formatter.hpp"
namespace sgn::retail
{
// Full4452E0 caller flow. The language store precedes the live indexed read.
// No phase==1 means no App read, formatter entry or source state write.
void select_source_level_script_4452e0(RaceSessionPlatform &, SourceScriptPathCall &);
void select_source_level_script_4452e0(RaceSessionPlatform &);
} // namespace sgn::retail
