#include "sgn/live_scene_geometry.hpp"
#include "sgn/retail_camera_transform.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <array>
#include <bit>
#include <map>

namespace sgn
{
LiveBlend source_polygon_blend(std::uint8_t flags, bool force_translucent) noexcept
{
    if (!(flags & 0x40) && !force_translucent)
        return LiveBlend::opaque;
    switch (flags & 0x78)
    {
    case 0x70:
        return LiveBlend::multiply;
    case 0x58:
        return LiveBlend::add;
    case 0x48:
    case 0x50:
        return LiveBlend::alpha;
    default:
        return LiveBlend::alpha_add;
    }
}
std::uint32_t source_polygon_color(std::uint32_t argb, std::uint8_t alpha, std::uint8_t flags,
                                   bool force_translucent) noexcept
{
    if (!(flags & 0x40) && !force_translucent)
        return argb | 0xff000000u;                                                              // 4334C0
    return (flags & 0x78) == 0x50 ? argb : (argb & 0x00ffffffu) | (std::uint32_t(alpha) << 24); // 4338E0
}
LiveGeometry strip_live_geometry(const retail::VehicleStripGeometry &source)
{
    if (source.polygon_count > source.polygons.size() || source.corner_count > source.corners.size() ||
        source.vertex_count > source.vertices.size())
        throw FormatError("source strip active span outside owner");
    LiveGeometry out;
    out.model.parts = 1;
    out.model.lod_count = 1;
    // The game table4955E8 row5 names49579C;42C510/42C340 load this before PMDs.
    out.model.textures = {"Misc\\Skidmark2"};
    out.model.vertices.reserve(std::size_t(source.polygon_count) * 6);
    for (unsigned q = 0; q < source.polygon_count; ++q)
    {
        const auto &p = source.polygons[q];
        if (p.vertex_count != 4 || p.texture != 5 || p.normal >= source.normals.size() ||
            p.first_corner > source.corner_indices.size() - 4)
            throw FormatError("source strip polygon outside recovered quad contract");
        std::array<Vertex, 4> corners;
        for (unsigned j = 0; j < 4; ++j)
        {
            const auto ci = source.corner_indices[p.first_corner + j];
            if (ci >= source.corner_count)
                throw FormatError("source strip corner outside active span");
            const auto &c = source.corners[ci];
            if (c.vertex >= source.vertex_count)
                throw FormatError("source strip vertex outside active span");
            const auto &v = source.vertices[c.vertex];
            const auto &n = source.normals[p.normal];
            for (float f : {v.x, v.y, v.z, n.x, n.y, n.z, c.u, c.v})
                if (!std::isfinite(f))
                    throw FormatError("nonfinite live strip coordinate");
            corners[j] = {
                {v.x, v.y, v.z}, {n.x, n.y, n.z}, c.u, c.v, source_polygon_color(c.color, p.field_02, p.flags)};
        }
        const auto first = static_cast<std::uint32_t>(out.model.vertices.size());
        // 477DD0: strip permutation1,0,2,3. D3D triangle-strip odd triangles
        //  reverse their first two vertices. Opaque4334C0 uses a fan instead.
        const std::array<unsigned, 6> order =
            (p.flags & 0x40) ? std::array<unsigned, 6>{1, 0, 2, 2, 0, 3} : std::array<unsigned, 6>{0, 1, 2, 0, 2, 3};
        for (auto j : order)
            out.model.vertices.push_back(corners[j]);
        out.model.surfaces.push_back({first, 6, 0, (p.flags & 0x40) != 0, (p.flags & 0x80) != 0, 0});
        out.materials.push_back({source_polygon_blend(p.flags)});
    }
    if (!out.model.vertices.empty())
    {
        out.model.minimum = out.model.maximum = out.model.vertices.front().position;
        for (const auto &v : out.model.vertices)
        {
            auto &a = out.model.minimum;
            auto &b = out.model.maximum;
            a = {std::min(a.x, v.position.x), std::min(a.y, v.position.y), std::min(a.z, v.position.z)};
            b = {std::max(b.x, v.position.x), std::max(b.y, v.position.y), std::max(b.z, v.position.z)};
        }
    }
    return out;
}
namespace
{
retail::CameraMatrix part_rotation(const retail::ModelTransform &transform)
{ // 44FF90
    using namespace retail;
    const auto z = transform.forward;
    VehicleVector y{transform.up.x, transform.up.y, transform.up.z, 0};
    const auto product = [&] { return (double(y.z) * z.z + double(y.y) * z.y) + double(y.x) * z.x; };
    if (std::abs(product()) > std::bit_cast<float>(std::uint32_t{0x3f7fff58}))
    {
        y.z = vehicle_f32(double(y.z) + 1);
        y = vehicle_normalize(y);
    }
    const double d = product();
    y.x = vehicle_f32(double(y.x) - d * z.x);
    y.y = vehicle_f32(double(y.y) - d * z.y);
    y.z = vehicle_f32(double(y.z) - d * z.z);
    y = vehicle_normalize(y);
    // Preserve the source subtract-then-negate operations, including signed0.
    const float x = vehicle_f32(-(double(y.z) * z.y - double(z.z) * y.y));
    const float b = -vehicle_f32(double(z.z) * y.x - double(y.z) * z.x);
    const float c = -vehicle_f32(double(y.y) * z.x - double(z.y) * y.x);
    return {x, b, c, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, 0, 0, 0, 1};
}
} // namespace
LiveModelViews model_live_views(retail::ModelResource &model, unsigned lod, const retail::CameraState &camera,
                                const retail::CameraModelTransform &model_transform)
{
    using namespace retail;
    const auto count = static_cast<unsigned>(model.part_count(lod));
    std::map<const ModelTransform *, CameraMatrix> rotations;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto p = model.part(lod, i);
        rotations[&p.transform] = p.flags == 0 ? vehicle_identity() : part_rotation(p.transform);
    }
    LiveModelViews result;
    result.parts.reserve(count);
    result.model_to_view.reserve(count);
    for (unsigned i = 0; i < count; ++i)
    {
        const auto p = model.part(lod, i);
        const auto &t = p.transform;
        const CameraModelTransform part{{t.position.x, t.position.y, t.position.z, 0},
                                        {t.scale.x, t.scale.y, t.scale.z, 0},
                                        {p.pivot.x, p.pivot.y, p.pivot.z, 0},
                                        rotations.at(&t)};
        const auto m = camera_model_view(camera, model_transform, count, &part);
        for (float f : m)
            if (!std::isfinite(f))
                throw FormatError("nonfinite source model-to-view transform");
        result.parts.push_back({{}, i == 0 || p.flags == 1});
        result.model_to_view.push_back(m);
    }
    return result;
}
LiveModelViews model_live_views(retail::ModelResource &model, unsigned lod, const retail::CameraState &camera,
                                const retail::CameraModelTransform &outer, const retail::ViewMatrixArena &arena,
                                const retail::CameraMatrix &identity)
{
    using namespace retail;
    const auto count = static_cast<unsigned>(model.part_count(lod));
    LiveModelViews result;
    result.parts.reserve(count);
    result.model_to_view.reserve(count);
    for (unsigned i = 0; i < count; ++i)
    {
        const auto p = model.part(lod, i);
        const auto &t = p.transform;
        const CameraModelTransform part{{t.position.x, t.position.y, t.position.z, 0},
                                        {t.scale.x, t.scale.y, t.scale.z, 0},
                                        {p.pivot.x, p.pivot.y, p.pivot.z, 0},
                                        resolve_model_matrix(arena, identity, t.tail[0])};
        const auto m = camera_model_view(camera, outer, count, &part);
        for (float f : m)
            if (!std::isfinite(f))
                throw FormatError("nonfinite published model-to-view transform");
        result.parts.push_back({{}, i == 0 || p.flags == 1});
        result.model_to_view.push_back(m);
    }
    return result;
}
LiveGeometry model_live_geometry(retail::ModelResource &source, unsigned lod)
{
    const auto &authored = source.geometry(lod);
    LiveGeometry out;
    out.model.parts = authored.parts;
    out.model.lod_count = authored.lod_count;
    out.model.kind = authored.kind;
    out.model.textures = authored.textures;
    out.model.vertices.reserve(authored.vertices.size());
    const auto indices = source.source_corner_indices();
    for (unsigned part = 0; part < out.model.parts; ++part)
    {
        const auto attributes = source.source_attributes(lod, part);
        const auto coordinates = source.source_coordinates(lod, part);
        const auto vertices = source.source_vertices(lod, part);
        const auto flags02 = source.source_part_flags_02(lod, part);
        const bool forced = (source.source_model_flags_02() & 8) && (flags02 & 0x80) && (flags02 >> 8) != 0;
        for (const auto &a : attributes)
        {
            const unsigned n = a[0], texture = a[1], first = unsigned(a[6]) | (unsigned(a[7]) << 8);
            const bool translucent = (a[3] & 0x40) || forced;
            if (n < 3 || n > 15 || texture >= out.model.textures.size() || first > indices.size() ||
                n > indices.size() - first)
                throw FormatError("live PMD polygon outside source topology contract");
            std::array<Vertex, 15> corners;
            for (unsigned j = 0; j < n; ++j)
            {
                const auto index = indices[first + j];
                if (index >= coordinates.size())
                    throw FormatError("live PMD corner outside source part");
                const auto &c = coordinates[index];
                const auto vertex = c[2] & 0xffff;
                if (vertex >= vertices.size())
                    throw FormatError("live PMD vertex outside source part");
                const auto &v = vertices[vertex];
                const float u = std::bit_cast<float>(c[0]), vv = std::bit_cast<float>(c[1]);
                for (float f : {v.x, v.y, v.z, u, vv})
                    if (!std::isfinite(f))
                        throw FormatError("nonfinite live PMD coordinate");
                corners[j] = {{v.x, v.y, v.z}, {}, u, vv, source_polygon_color(c[3], a[2], a[3], forced)};
            }
            const auto at = static_cast<std::uint32_t>(out.model.vertices.size());
            if (translucent)
            {
                // Read-only closed form of the pinned477CD0 table for3..15.
                std::array<unsigned, 15> order{1, 0, 2};
                for (unsigned j = 3; j < n; ++j)
                    order[j] = (j & 1) ? n - (j - 1) / 2 : (j / 2) + 1;
                for (unsigned j = 0; j + 2 < n; ++j)
                {
                    const std::array triangle{order[j + (j & 1)], order[j + 1 - (j & 1)], order[j + 2]};
                    for (auto k : triangle)
                        out.model.vertices.push_back(corners[k]);
                }
            }
            else
                for (unsigned j = 1; j + 1 < n; ++j)
                    for (auto k : {0u, j, j + 1})
                        out.model.vertices.push_back(corners[k]);
            out.model.surfaces.push_back({at, 3 * (n - 2), texture, translucent, (a[3] & 0x80) != 0, part});
            out.materials.push_back({source_polygon_blend(a[3], forced)});
        }
    }
    if (!out.model.vertices.empty())
    {
        out.model.minimum = out.model.maximum = out.model.vertices.front().position;
        for (const auto &v : out.model.vertices)
        {
            auto &a = out.model.minimum;
            auto &b = out.model.maximum;
            a = {std::min(a.x, v.position.x), std::min(a.y, v.position.y), std::min(a.z, v.position.z)};
            b = {std::max(b.x, v.position.x), std::max(b.y, v.position.y), std::max(b.z, v.position.z)};
        }
    }
    return out;
}
} // namespace sgn
