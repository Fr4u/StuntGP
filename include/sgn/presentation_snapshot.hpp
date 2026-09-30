#pragma once
#include "sgn/renderer.hpp"
#include "sgn/live_scene_geometry.hpp"

namespace sgn
{
// New platform presentation data, captured only after an accepted source tick.
// There are no Session pointers, resource-owner references or borrowed spans.
struct PresentationObject
{
    std::uint64_t identity{};   // stable draw entity, independent of selected LOD
    std::uint64_t continuity{}; // increment on an actual object cut/teleport/reset
    std::size_t mesh{};
    Pose pose{};
    std::vector<SceneInstance::Part> parts;
    std::vector<std::array<float, 16>> model_to_view;
    std::shared_ptr<const LiveGeometry> geometry;
};
struct PresentationSnapshot
{
    std::uint64_t tick{};
    std::uint64_t camera_continuity{}; // new epoch on a source camera cut
    View view;
    std::vector<PresentationObject> objects;
};
using ImmutablePresentationSnapshot = std::shared_ptr<const PresentationSnapshot>;
} // namespace sgn
