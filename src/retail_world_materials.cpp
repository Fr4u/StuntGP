#include "sgn/retail_world_materials.hpp"
#include "sgn/format_require.hpp"
#include <limits>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
const WorldMaterialLodView &lod(const WorldMaterialModelView &m, std::size_t index)
{
    require(index < m.lods.size() && m.lods[index], "material source LOD pointer outside owner");
    require(m.part_count <= m.lods[index]->parts.size(), "material source part count outside LOD owner");
    return *m.lods[index];
}

} // namespace
bool world_material_part_already_visited(const WorldMaterialModelView &model, std::uint32_t level, std::uint32_t part)
{
    const auto &view = lod(model, level);
    require(part < model.part_count, "material source part selector outside owner");
    // 42B8C8..42B8DF tests identity of part+34, not header contents or LOD identity.
    const auto *identity = view.parts[part].headers.data();
    for (std::size_t previous = 0; previous < part; ++previous)
        if (view.parts[previous].headers.data() == identity && !view.parts[previous].headers.empty())
            return true;
    return false;
}
void add_world_material_base(const WorldMaterialModelView &model, std::uint32_t base)
{
    require(model.lods.size() <= std::numeric_limits<std::uint32_t>::max(), "material LOD counter overflow");
    for (std::size_t level = 0; level < model.lods.size(); ++level)
    {
        const auto &view = lod(model, level);
        for (std::uint32_t part = 0; part < model.part_count; ++part)
        {
            if (world_material_part_already_visited(model, static_cast<std::uint32_t>(level), part))
                continue;
            for (auto &header : view.parts[part].headers)
                header[1] = static_cast<std::uint8_t>(header[1] + static_cast<std::uint8_t>(base));
        }
    }
}
void replace_world_material_id(WorldMaterialRegistry registry, std::uint32_t from, std::uint32_t to)
{
    for (const auto &entry : registry)
    {
        if (!entry.model)
            break;
        const auto &model = *entry.model;
        for (std::size_t level = 0; level < model.lods.size(); ++level)
        {
            const auto &view = lod(model, level);
            for (std::uint32_t part = 0; part < model.part_count; ++part)
                for (auto &header : view.parts[part].headers)
                    if (std::uint32_t(header[1]) == from)
                        header[1] = static_cast<std::uint8_t>(to);
        }
    }
}
std::int32_t world_material_name_lookup(WorldMaterialNames names, const std::string &requested)
{
    std::int32_t result = 1;
    for (const auto *name : names)
    {
        if (!name)
            break;
        result = compare_material_names(BorrowedMaterialName(name), BorrowedMaterialName(requested));
        if (result == 0)
            break;
    }
    return result;
}
void remap_world_material_names(WorldMaterialRegistry registry, WorldMaterialNames logical, WorldMaterialNames unique)
{
    require(logical.size() <= std::numeric_limits<std::uint32_t>::max(), "material logical counter overflow");
    require(unique.size() <= std::numeric_limits<std::uint32_t>::max(), "material unique counter overflow");
    for (std::size_t from = 0; from < logical.size() && logical[from]; ++from)
    {
        std::size_t to = 0;
        while (to < unique.size() && unique[to] &&
               compare_material_names(BorrowedMaterialName(logical[from]), BorrowedMaterialName(unique[to])) != 0)
            ++to;
        require(to < unique.size() && unique[to], "material logical name has no source unique-table match");
        if (from != to)
            replace_world_material_id(registry, static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(to));
    }
}
void begin_world_material_batch(WorldMaterialNamespace &state)
{
    require(state.logical_names.size() <= std::numeric_limits<std::uint32_t>::max(), "material request count overflow");
    state.logical_base = static_cast<std::uint32_t>(state.logical_names.size());
    state.batch_upload_count = 0;
}
std::uint32_t next_world_material_motion_index(const WorldMaterialNamespace &state) noexcept
{
    return state.physical_upload_count < state.sky_material_base ? 0 : state.batch_upload_count;
}
void publish_world_material_request_42c03c(WorldMaterialNamespace &state, const BorrowedMaterialName &name)
{
    state.batch_upload_count += 1;
    state.physical_upload_count += 1;
    state.unique_names.push_back(name);
    state.logical_names.push_back(name);
}
void commit_world_material_request(WorldMaterialNamespace &state, const BorrowedMaterialName &name,
                                   std::optional<WorldMaterialUpload> upload)
{
    const bool fresh = world_material_name_lookup(state.logical_names, name) != 0;
    require(fresh == upload.has_value(), "material upload result disagrees with source raw-name deduplication");
    // The source name arrays contain resource pointers. Preserve identity and
    // current string contents instead of silently canonicalizing/copying labels.
    if (fresh)
    {
        state.materials.prepare_next(next_world_material_motion_index(state));
        state.materials.complete_next(upload->primary_texture, upload->alternate_texture, upload->dimension);
        publish_world_material_request_42c03c(state, name);
        return;
    }
    state.logical_names.push_back(name);
}

std::int32_t world_material_name_lookup(BorrowedMaterialNames names, const BorrowedMaterialName &requested)
{
    std::int32_t result = 1;
    for (const auto &name : names)
    {
        if (!name)
            break;
        result = compare_material_names(name, requested);
        if (result == 0)
            break;
    }
    return result;
}
void remap_world_material_names(WorldMaterialRegistry registry, BorrowedMaterialNames logical,
                                BorrowedMaterialNames unique)
{
    require(logical.size() <= std::numeric_limits<std::uint32_t>::max(), "material logical counter overflow");
    require(unique.size() <= std::numeric_limits<std::uint32_t>::max(), "material unique counter overflow");
    for (std::size_t from = 0; from < logical.size() && logical[from]; ++from)
    {
        std::size_t to = 0;
        while (to < unique.size() && unique[to] && compare_material_names(logical[from], unique[to]) != 0)
            ++to;
        require(to < unique.size() && bool(unique[to]), "material logical name has no source unique-table match");
        if (from != to)
            replace_world_material_id(registry, static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(to));
    }
}
void publish_world_material_request_42c03c(WorldMaterialNamespace &state, const std::string &name)
{
    publish_world_material_request_42c03c(state, BorrowedMaterialName(name));
}
void commit_world_material_request(WorldMaterialNamespace &state, const std::string &name,
                                   std::optional<WorldMaterialUpload> upload)
{
    commit_world_material_request(state, BorrowedMaterialName(name), upload);
}
} // namespace sgn::retail
