#pragma once
namespace sgn::retail
{
class RaceSessionPlatform;
// Called at442841 after the original429510 numeric setup, once per source
// simulation iteration. No elapsed time or monitor cadence input.
// Actual405210 on the sole controls/vehicle/CAM/query owners. All four
// source controllers are visited in order; no per-level Session, geometry copy
// or provider fallback. Full mode7 uses the actual PMD4215B0 ground query.
// Reached invalid source addresses/numeric domains throw after prior writes.
void advance_source_camera_405210(RaceSessionPlatform &);
} // namespace sgn::retail
