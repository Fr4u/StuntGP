#pragma once
#include "sgn/source_race_scene_small_children.hpp"
namespace sgn::retail
{
// Actual462BB0 ->462BD2 positive finite base, non-special exponent path;
// FYL2X/466230/466319 with original argument spills and CW/SW behavior.
// The same CRT's636528 is read on every invocation. Other reached branches
// remain instruction-specific exceptions; no std::pow or table replacement.
class SourceSceneCrtPower final : public SourceSceneSmallRequired
{
  public:
    SourceSceneExtended80 power_462bb0(FrontendState &, NativeSourceMenuCrt &, std::uint32_t callsite,
                                       const SourceScenePowerArguments &) override;
};
// Actual Composition consumer for the small-children router, including colors.
// The receiver is stateless; this borrows existing Core owners synchronously.
bool perform_source_race_scene_small_child_with_crt_power(SceneOperation, FrontendState &, SourceBootFilePlatform &,
                                                          NativeSourceMenuCrt &);
} // namespace sgn::retail
