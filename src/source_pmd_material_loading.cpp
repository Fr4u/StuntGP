#include "sgn/source_pmd_material_loading.hpp"
#include "sgn/source_pmd_material_base.hpp"
#include "sgn/scene_resources.hpp"
#include <limits>

namespace sgn::retail
{
namespace
{
std::optional<SourcePmdModelView> reached_model(SceneResources &scene, unsigned slot)
{
    const auto records = scene.source_records();
    if (slot == records.size())
    {
        if (scene.material_registry_terminal_636518() != 0)
            throw FormatError("nonzero source material terminal is outside the admitted registry owner");
        return std::nullopt; // 42BA80 reached636518 after slot15
    }
    if (slot > records.size())
        throw FormatError("material registry traversal passed source terminal");
    const auto &descriptor = records[slot].descriptor;
    if (std::holds_alternative<std::monostate>(descriptor))
        return std::nullopt;
    const auto *binding = std::get_if<ScenePmdBinding>(&descriptor);
    if (!binding)
        throw FormatError("material traversal reached a non-PMD descriptor");
    return scene.models().source_at(binding->handle);
}
} // namespace
void replace_source_pmd_material_id_42ba00(SceneResources &scene, std::uint32_t from, std::uint32_t to)
{
    scene.require_owner_thread();
    // 636438 is registry slot2. Flags+04 never participate in42BA00.
    for (unsigned slot = 2;; ++slot)
    {
        const auto reached = reached_model(scene, slot);
        if (!reached)
            return;
        const auto &model = *reached;
        const auto levels = model.maximum_detail_level() + std::uint32_t{1}; // 42BA1F..25
        if (levels == 0)
            continue;
        const auto count = model.part_count(); // ECX captured42BA27, retained across LODs
        for (std::uint32_t level = 0; level < levels; ++level)
        {
            if (level >= 4)
                throw FormatError("material replacement LOD exceeds four source slots");
            (void)model.descriptor().field<std::uint32_t>(0x48 + level * 4).value(); // 42BA33 even count0
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto part = model.part(level, index);
                (void)model.bytes().field<std::uint32_t>(part.location().offset + 0x34).value(); // 42BA3E before count
                const auto polygons = part.attribute_count().value();                            // 42BA43 low16
                for (std::uint32_t polygon = 0; polygon < polygons; ++polygon)
                {
                    const auto id = part.attribute_byte(polygon, 1);
                    if (std::uint32_t(id.value()) == from)
                        id.set(static_cast<std::uint8_t>(to)); // 42BA4D..57
                }
            }
        }
    }
}
void remap_source_pmd_material_names_42ba90(SceneResources &scene, const WorldMaterialNamespace &materials)
{
    scene.require_owner_thread();
    const auto &logical = materials.logical_names;
    const auto &unique = materials.unique_names;
    if (logical.size() > std::numeric_limits<std::uint32_t>::max() ||
        unique.size() > std::numeric_limits<std::uint32_t>::max())
        throw FormatError("material name table exceeds source DWORD index");
    for (std::size_t from = 0; from < logical.size() && logical[from]; ++from)
    {
        std::size_t to = 0;
        // 42BAB5..42BB32: first byte-equal name, read from its current owner.
        for (;; ++to)
        {
            if (to >= unique.size() || !unique[to])
                throw FormatError("reached material remap name has no physical-table match");
            if (compare_material_names(logical[from], unique[to]) == 0)
                break;
        }
        if (from != to)
            replace_source_pmd_material_id_42ba00(scene, static_cast<std::uint32_t>(from),
                                                  static_cast<std::uint32_t>(to));
        // Sequential replacements, including aliases and flagged registry rows.
        // A precomputed simultaneous map would change the source behavior.
    }
}
void load_source_pmd_world_materials_42c510(SceneResources &scene, std::span<const std::uint32_t, 16> flags,
                                            WorldMaterialLoadingState &state, WorldMaterialLoadingContext &context,
                                            MaterialFileSource &files, MaterialTextureBackend &backend,
                                            WorldMaterialLoadingPresentation *presentation)
{
    scene.require_owner_thread();
    reset_material_reload_42bdd0(state.reload); // 42C510 before presentation/reset
    if (context.loading_presentation_enabled == 1 && context.loading_already_shown != 1)
    {
        if (!presentation)
            throw MaterialSourceFailure(0x42c51e, 0, "required source loading presentation has no consumer");
        presentation->run(state, context, files, backend);
    }
    reset_world_material_loading(state, backend); // same physical owner, real release
    load_world_material_prelude(source_world_material_prelude(context.scene), state, context, files, backend);
    for (unsigned slot = 2;; ++slot)
    {
        // 42C541/568 tests descriptor identity before inspecting flags. A flagged
        // row need not have a live PMD payload: source skips its dereference.
        const auto records = scene.source_records();
        if (slot == records.size())
        {
            if (scene.material_registry_terminal_636518() != 0)
                throw FormatError("nonzero source material terminal is outside the admitted registry owner");
            break; // 42C568 reached636518 after slot15; no flags/PMD dereference
        }
        if (slot > records.size())
            throw FormatError("material registry traversal passed source terminal");
        if (std::holds_alternative<std::monostate>(records[slot].descriptor))
            break;
        if (flags[slot] != 0)
            continue;
        const auto model = reached_model(scene, slot);
        const SourcePmdMaterialNames names(*model); // 42C559 captures table, no eager string copy
        load_source_pmd_material_requests_42c440(names, state, context, files, backend);
        // 42C561 reloads registry descriptor after upload callbacks.
        bind_source_pmd_material_base_42c563(scene, slot, state.names);
    }
    remap_source_pmd_material_names_42ba90(scene, state.names); // tail JMP42C573
}
} // namespace sgn::retail
