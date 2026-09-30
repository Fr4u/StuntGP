#include "sgn/source_pmd_geometry.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>

namespace sgn
{
namespace
{
using sgn::format_check::require;
std::string texture_path(std::string raw, std::string_view prefix)
{
    // 42BB70 tests the full case-sensitive input for Trk_, then substitutes
    // the backslash basename. A leading backslash at index0 is retained.
    if (prefix.empty() || raw.find("Trk_") == raw.npos)
        return raw;
    const auto last = raw.find_last_of('\\');
    const auto first = last == raw.npos || last == 0 ? 0 : last + 1;
    require(raw.size() - first < 64, "source42BB70 basename exceeds64-byte local buffer");
    return std::string(prefix) + "\\" + raw.substr(first);
}
retail::CameraMatrix rotation(const retail::SourcePmdTransform &transform)
{
    using namespace retail;
    const auto z = transform.forward;
    VehicleVector y{transform.up.x, transform.up.y, transform.up.z, 0};
    const auto dot = [&] { return (double(y.z) * z.z + double(y.y) * z.y) + double(y.x) * z.x; };
    if (std::abs(dot()) > std::bit_cast<float>(0x3f7fff58u))
    {
        y.z = vehicle_f32(double(y.z) + 1);
        y = vehicle_normalize(y);
    }
    const double d = dot();
    y.x = vehicle_f32(double(y.x) - d * z.x);
    y.y = vehicle_f32(double(y.y) - d * z.y);
    y.z = vehicle_f32(double(y.z) - d * z.z);
    y = vehicle_normalize(y);
    const float x = vehicle_f32(-(double(y.z) * z.y - double(z.z) * y.y));
    const float b = -vehicle_f32(double(z.z) * y.x - double(y.z) * z.x);
    const float c = -vehicle_f32(double(y.y) * z.x - double(z.y) * y.x);
    return {x, b, c, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, 0, 0, 0, 1};
}
void bounds(LiveGeometry &out)
{
    if (out.model.vertices.empty())
        return;
    out.model.minimum = out.model.maximum = out.model.vertices.front().position;
    for (const auto &v : out.model.vertices)
    {
        auto &a = out.model.minimum;
        auto &b = out.model.maximum;
        a = {std::min(a.x, v.position.x), std::min(a.y, v.position.y), std::min(a.z, v.position.z)};
        b = {std::max(b.x, v.position.x), std::max(b.y, v.position.y), std::max(b.z, v.position.z)};
    }
}
} // namespace
SourcePmdGeometryAdapter::SourcePmdGeometryAdapter(retail::SourcePmdModelView source, std::string prefix)
    : source_(std::move(source)), track_texture_prefix_(std::move(prefix))
{
    source_.check();
    require(track_texture_prefix_.size() < 64 && track_texture_prefix_.find('\0') == std::string::npos,
            "source track texture prefix domain");
}
unsigned SourcePmdGeometryAdapter::source_lod_index(unsigned slot) const
{
    return source_.source_lod_index(slot);
}
LiveGeometry SourcePmdGeometryAdapter::capture(unsigned source_slot) const
{
    const auto lod = source_lod_index(source_slot);
    const auto count = source_.part_count();
    LiveGeometry out;
    out.model.kind = source_.kind();
    out.model.parts = count;
    out.model.lod_count = source_.maximum_detail_level() + 1;
    const auto texture_count = source_.texture_count();
    out.model.textures.reserve(texture_count);
    for (std::uint32_t i = 0; i < texture_count; ++i)
        out.model.textures.push_back(texture_path(source_.texture_name(i), track_texture_prefix_));
    const auto indices = source_.corner_indices();
    const auto model_flags = source_.descriptor().field<std::uint16_t>(2).value();
    for (unsigned part = 0; part < count; ++part)
    {
        const auto record = source_.part(lod, part);
        const auto attributes = record.attributes();
        const auto coordinates = record.coordinates();
        const auto vertices = record.vertices();
        const auto flags = record.flags_02().value();
        const bool forced = (model_flags & 8u) != 0 && (flags & 0x80u) != 0 && (flags >> 8) != 0;
        for (std::uint32_t polygon = 0; polygon < attributes.size(); ++polygon)
        {
            const auto a = attributes.at(polygon).value();
            const auto n = std::uint32_t(a[0]), texture = std::uint32_t(a[1]);
            const auto first = std::uint32_t(a[6]) | (std::uint32_t(a[7]) << 8);
            require(n >= 3 && n <= 15 && texture < texture_count && first <= indices.size() &&
                        n <= indices.size() - first,
                    "source PMD mesh polygon topology domain");
            std::array<Vertex, 15> corners;
            for (std::uint32_t j = 0; j < n; ++j)
            {
                const auto c = coordinates.at(indices.at(first + j).value()).value();
                const auto v = vertices.at(c[2] & 0xffffu).value();
                const float u = std::bit_cast<float>(c[0]), vv = std::bit_cast<float>(c[1]);
                for (float f : {v.x, v.y, v.z, u, vv})
                    require(std::isfinite(f), "nonfinite source PMD mesh coordinate");
                // The LiveGeometry source-color shader does not use NORMAL.
                // Zero here is unused host transport padding, not recovered lighting.
                corners[j] = {{v.x, v.y, v.z}, {}, u, vv, source_polygon_color(c[3], a[2], a[3], forced)};
            }
            const auto required = 3 * (n - 2);
            constexpr auto max_vertices = std::numeric_limits<std::uint32_t>::max() / sizeof(Vertex);
            require(out.model.vertices.size() <= max_vertices && required <= max_vertices - out.model.vertices.size(),
                    "source PMD GPU transport exceeds32-bit byte capacity");
            const auto at = static_cast<std::uint32_t>(out.model.vertices.size());
            const bool translucent = (a[3] & 0x40u) != 0 || forced;
            if (translucent)
            {
                // Exact count-indexed PE477CD0+n*64 rows3..15, closed form;
                // D3D strip odd-triangle parity is expanded for the existing triangle-list transport.
                std::array<std::uint32_t, 15> order{1, 0, 2};
                for (std::uint32_t j = 3; j < n; ++j)
                    order[j] = (j & 1u) ? n - (j - 1) / 2 : j / 2 + 1;
                for (std::uint32_t j = 0; j + 2 < n; ++j)
                {
                    const std::array triangle{order[j + (j & 1u)], order[j + 1 - (j & 1u)], order[j + 2]};
                    for (auto index : triangle)
                        out.model.vertices.push_back(corners[index]);
                }
            }
            else
                for (std::uint32_t j = 1; j + 1 < n; ++j)
                    for (auto index : {0u, j, j + 1})
                        out.model.vertices.push_back(corners[index]);
            out.model.surfaces.push_back({at, required, texture, translucent, (a[3] & 0x80u) != 0, part});
            out.materials.push_back({source_polygon_blend(a[3], forced)});
        }
    }
    bounds(out);
    return out;
}
LiveGeometry model_live_geometry(retail::ModelResources &models, std::size_t handle, unsigned slot,
                                 std::string_view prefix)
{
    if (models.source_backed(handle))
        return SourcePmdGeometryAdapter(models.source_at(handle), std::string(prefix)).capture(slot);
    return model_live_geometry(models.at(handle), models.source_lod_index(handle, slot));
}
LiveModelViews model_live_views(retail::ModelResources &models, std::size_t handle, unsigned slot,
                                const retail::CameraState &camera, const retail::CameraModelTransform &outer,
                                const retail::ViewMatrixArena &arena, const retail::CameraMatrix &identity)
{
    if (models.source_backed(handle))
        return SourcePmdGeometryAdapter(models.source_at(handle), "")
            .capture_published_views(slot, camera, outer, arena, identity);
    return model_live_views(models.at(handle), models.source_lod_index(handle, slot), camera, outer, arena, identity);
}
LiveModelViews SourcePmdGeometryAdapter::capture_published_views(unsigned slot, const retail::CameraState &camera,
                                                                 const retail::CameraModelTransform &outer,
                                                                 const retail::ViewMatrixArena &arena,
                                                                 const retail::CameraMatrix &identity) const
{
    using namespace retail;
    const auto lod = source_lod_index(slot), count = source_.part_count();
    LiveModelViews out;
    out.parts.reserve(count);
    out.model_to_view.reserve(count);
    for (unsigned i = 0; i < count; ++i)
    {
        const auto part = source_.part(lod, i);
        const auto t = part.transform().value();
        const auto pivot = part.pivot().value();
        const CameraModelTransform local{{t.position.x, t.position.y, t.position.z, t.position.w},
                                         {t.scale.x, t.scale.y, t.scale.z, t.scale.w},
                                         {pivot.x, pivot.y, pivot.z, pivot.w},
                                         resolve_model_matrix(arena, identity, t.tail[0])};
        const auto matrix = camera_model_view(camera, outer, count, &local);
        for (float f : matrix)
            require(std::isfinite(f), "nonfinite published canonical model-to-view transport");
        out.parts.push_back({{}, i == 0 || part.flags().value() == 1});
        out.model_to_view.push_back(matrix);
    }
    return out;
}
LiveModelViews SourcePmdGeometryAdapter::capture_views(unsigned source_slot, const retail::CameraState &camera,
                                                       const retail::CameraModelTransform &outer) const
{
    using namespace retail;
    const auto lod = source_lod_index(source_slot), count = source_.part_count();
    // One model allocation: offset is sufficient identity inside this call.
    // Only presentation matrices are owned here; no transform copy is retained.
    std::map<std::uint32_t, CameraMatrix> rotations;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto part = source_.part(lod, i);
        const auto transform = part.transform();
        rotations[transform.location().offset] =
            part.flags().value() == 0 ? vehicle_identity() : rotation(transform.value());
    }
    LiveModelViews out;
    out.parts.reserve(count);
    out.model_to_view.reserve(count);
    for (unsigned i = 0; i < count; ++i)
    {
        const auto part = source_.part(lod, i);
        const auto field = part.transform();
        const auto transform = field.value();
        const auto pivot = part.pivot().value();
        const CameraModelTransform local{{transform.position.x, transform.position.y, transform.position.z, 0},
                                         {transform.scale.x, transform.scale.y, transform.scale.z, 0},
                                         {pivot.x, pivot.y, pivot.z, 0},
                                         rotations.at(field.location().offset)};
        const auto matrix = camera_model_view(camera, outer, count, &local);
        for (float f : matrix)
            require(std::isfinite(f), "nonfinite source PMD model-view transport");
        out.parts.push_back({{}, i == 0 || part.flags().value() == 1});
        out.model_to_view.push_back(matrix);
    }
    return out;
}
} // namespace sgn
