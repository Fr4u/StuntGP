#pragma once
#include "sgn/polygon_surface_recipes.hpp"
#include "sgn/renderer.hpp"
#include <optional>

namespace sgn
{
// NEW diagnostic value. Existing immutable owners plus an exact copied ramp.
// Release the value before the material suspension external-frame guard.
struct CapturedPolygonFrontRecipe
{
    ImmutablePolygonSubmission submission;
    SdrGammaRamp gamma;
    std::uint64_t front_image{}; // Process-local correlation, not a portable ID.
    unsigned width{}, height{};
};
namespace diagnostics
{
// CPU-only selection/validation shared by the real accessor and bounded tests.
// The caller must first enforce its existing owner-thread/idle/bound contract.
inline std::optional<CapturedPolygonFrontRecipe> select_polygon_front_snapshot(const PolygonSurfaceRecipes &recipes,
                                                                               const PolygonSurfaceRoles &current,
                                                                               const PolygonSurfaceRoles &expected,
                                                                               const PolygonDeviceIdentityRef &device)
{
    if (current.active != expected.active || current.front_image != expected.front_image ||
        current.back_image != expected.back_image || current.width != expected.width ||
        current.height != expected.height)
        throw FormatError("front snapshot requires synchronized image roles and extent");
    const auto &front = recipes.submissions[0];
    if (bool(front) != (current.front_image != 0))
        throw FormatError("front snapshot CPU/GPU presence differs");
    if (!front)
        return std::nullopt;
    if (!device || front->device != device || !current.active || current.width == 0 || current.height == 0)
        throw FormatError("front snapshot device, extent or role differs");
    return CapturedPolygonFrontRecipe{front, recipes.ramps[0], current.front_image, current.width, current.height};
}
} // namespace diagnostics
} // namespace sgn
