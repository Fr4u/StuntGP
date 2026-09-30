#pragma once
#include "sgn/live_scene_geometry.hpp"
#include <limits>

namespace sgn
{
// New platform optimization. Combine adjacent triangle-list draws only when
// their entire draw state and vertex range agree. No sorting, vertex changes,
// source-model writes, or changes to transparent primitive order are allowed.
inline void coalesce_live_draws(LiveGeometry &geometry)
{
    auto &surfaces = geometry.model.surfaces;
    auto &materials = geometry.materials;
    if (surfaces.size() != materials.size())
        throw FormatError("live draw batching requires one material per surface");
    // Validate before mutation: a malformed input must not be partly compacted.
    for (const auto &draw : surfaces)
    {
        if (draw.first > geometry.model.vertices.size() || draw.count > geometry.model.vertices.size() - draw.first ||
            draw.count % 3 != 0 || draw.texture >= geometry.model.textures.size() || draw.part >= geometry.model.parts)
            throw FormatError("live draw batching input outside triangle-list bindings");
    }
    std::size_t used = 0;
    for (std::size_t index = 0; index < surfaces.size(); ++index)
    {
        const auto draw = surfaces[index];
        const auto material = materials[index];
        if (used != 0)
        {
            auto &previous = surfaces[used - 1];
            if (std::uint64_t(previous.first) + previous.count == draw.first && previous.texture == draw.texture &&
                previous.part == draw.part && previous.translucent == draw.translucent &&
                previous.two_sided == draw.two_sided && materials[used - 1].blend == material.blend &&
                draw.count <= std::numeric_limits<std::uint32_t>::max() - previous.count)
            {
                previous.count += draw.count;
                continue;
            }
        }
        surfaces[used] = draw;
        materials[used] = material;
        ++used;
    }
    surfaces.resize(used);
    materials.resize(used);
}
} // namespace sgn
