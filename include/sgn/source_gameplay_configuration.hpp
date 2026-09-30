#pragma once

namespace sgn::retail
{
class RaceSessionPlatform;
class NativeSourceMenuCrt;
struct SourceBootFilePlatform;
// 4457E0 ->445620 on the existing Core GameplayRules, boot cache and CRT.
// Source pending==0 does not clear values/budget/order or read the file.
// Positive loads require the actual CRT bound to this platform/App. No owner
// is constructed here. Native x87 uses the caller's actual control/status;
// 429510 remains the source caller's responsibility, never reset by scaling.
// Complete NUL-free CSV<=1MiB, name<64/value<256 and bounded4631A7 domain.
void ensure_source_gameplay_configuration_4457e0(RaceSessionPlatform &, SourceBootFilePlatform &,
                                                 NativeSourceMenuCrt &);
} // namespace sgn::retail
