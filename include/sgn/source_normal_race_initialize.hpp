#pragma once
#include "sgn/window_input_owner.hpp"
#include <cstdint>
namespace sgn::retail
{
class FrontendState;
struct SourceBootFilePlatform;
class NativeSourceMenuCrt;
// Original40D500 caller on existing source owners; no preview world/adapters.
// Reached403F40 writes preset4's literal W. Presets1/2/3 (split forces1)
// transport the saved-frame identity through a retained NEW relative stack
// owner, not a historical numeric ESP. The admitted camera/first-frame graph
// observes XYZ and copies W; no numeric/address interpretation is licensed.
// Invalid preset caller history remains a reached404217 boundary.
// Changed spatial mode reaches actual4621D0/462090 on the retained audio owner.
void initialize_source_normal_race_40d500(FrontendState &, SourceBootFilePlatform &, NativeSourceMenuCrt &,
                                          InputWindowHandles);
// Same40D500 body, but with the proven pause446AF5 caller provenance for W.
// The caller has already completed45F9D0; this does not perform resume itself.
void restart_source_normal_race_446af5(FrontendState &, SourceBootFilePlatform &, NativeSourceMenuCrt &,
                                       InputWindowHandles);
// Direct446BB0 tail target, with the previously captured physical-input vehicle.
// Does not apply404310's split override and does not invoke403F40 or write W.
void apply_source_pause_camera_preset(FrontendState &, std::uint32_t entry, std::uint32_t captured_vehicle);
} // namespace sgn::retail
