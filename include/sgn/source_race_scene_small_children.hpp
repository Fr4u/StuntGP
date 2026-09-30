#pragma once
#include "sgn/retail_scene_transition.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>
namespace sgn::retail
{
class FrontendState;
class NativeSourceMenuCrt;
struct SourceBootFilePlatform;
struct SourceSceneExtended80
{
    std::array<std::byte, 10> bytes;
};
struct SourceScenePowerArguments
{
    SourceSceneExtended80 st0, st1;
    std::uint16_t control, status;
};
struct SourceSceneSmallBoundary final : std::runtime_error
{
    const std::uint32_t source_callsite, required_child;
    SourceSceneSmallBoundary(std::uint32_t site, std::uint32_t child, const char *why)
        : std::runtime_error(why), source_callsite(site), required_child(child)
    {
    }
};
struct SourceSceneSmallRequired
{
    virtual ~SourceSceneSmallRequired() = default;
    // Actual462BB0 on the SAME initialized CRT and source x87 operands/CW/SW.
    // No default numeric answer, finite substitute or predicted color output.
    virtual SourceSceneExtended80 power_462bb0(FrontendState &, NativeSourceMenuCrt &, std::uint32_t callsite,
                                               const SourceScenePowerArguments &) = 0;
};
// False means unowned enum, before accessing any owner. True only after the
// actual operation returns. All recognized but incomplete paths throw.
// Composition supplies its existing Core frontend/files/CRT, never a new world.
bool perform_source_race_scene_small_child(SceneOperation, FrontendState &, SourceBootFilePlatform &,
                                           NativeSourceMenuCrt &, SourceSceneSmallRequired &);
// Same routing, with explicit462BB0 boundary. Four simple operations and level
// configuration can return within their documented inherited child domains.
bool perform_source_race_scene_small_child(SceneOperation, FrontendState &, SourceBootFilePlatform &,
                                           NativeSourceMenuCrt &);
} // namespace sgn::retail
