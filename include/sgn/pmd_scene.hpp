#pragma once
#include "sgn/scene.hpp"

namespace sgn
{
// Authored pose only. Runtime wheel/effect poses are supplied by the recovered
// vehicle system; this helper does not animate or advance any game state.
inline Pose authored_part_pose(const PmdPart &part, bool multipart)
{
    if (!multipart)
        return {};
    Vec3 up = part.up, forward = part.forward, right{1, 0, 0};
    if (part.flags != 0)
    {
        if (std::abs(dot(up, forward)) > 0.99999f)
        {
            up.z += 1;
            up = normal(up);
        }
        up = normal(up - forward * dot(up, forward));
        right = cross(up, forward);
    }
    else
    {
        up = {0, 1, 0};
        forward = {0, 0, 1};
    }
    return {right * part.scale.x, up * part.scale.y, forward * part.scale.z,
            part.pivot + part.position - right * part.pivot.x - up * part.pivot.y - forward * part.pivot.z, 1};
}
inline std::vector<SceneInstance::Part> authored_scene_parts(const PmdRig &rig)
{
    if (rig.parts.size() != rig.geometry.parts)
        throw FormatError("incomplete PMD rig");
    std::vector<SceneInstance::Part> result;
    result.reserve(rig.parts.size());
    for (std::size_t i = 0; i < rig.parts.size(); ++i)
    {
        const auto &part = rig.parts[i];
        auto pose = authored_part_pose(part, rig.parts.size() > 1);
        bool visible = (i == 0 || part.flags == 1);
        result.push_back({pose, visible});
    }
    return result;
}
} // namespace sgn
