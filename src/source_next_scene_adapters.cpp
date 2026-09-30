#include "sgn/session_world_materials.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/retail_font_resources.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
std::uint32_t find_primary_resource(const MaterialD3D12 &backend, std::uint32_t token)
{
    for (std::uint32_t i = 0; i < backend.resource_entries(); ++i)
        if (backend.resource_primary_token(i) == token)
            return i;
    throw FormatError("next-scene font atlas does not belong to the live GPU resource bank");
}
} // namespace
void SessionWorldMaterials::require_next_scene_adapter_resources(const MaterialD3D12 &backend) const
{
    const auto platform = require_registry_live();
    if (!source_registry_only_ || !platform->source_construction_only_entry() ||
        scene_ != &platform->source_scene_resources() || failed_ || !loaded_ || !fonts_loaded_ || !published_ ||
        !source_scene_tail_completed_ || loaded_backend_ != &backend)
        throw FormatError("next-scene adapters require the same completed material/font owners");
    const auto &bank = platform->source_allocations();
    if (!bank.initialized() || bank.source_words().active != 1)
        throw FormatError("next-scene adapters require the current active source allocation bank");
    for (unsigned slot = 2; slot < 16; ++slot)
        if (scene_->model_handle(slot) != loaded_models_[slot - 2])
            throw FormatError("next-scene model registry changed after material loading");
    const auto &physical = state_.names.materials;
    // This is the actual D3D12 drawing domain. Source format!=1 retains old
    // font atlas indices (4327EE); those are not proof of new GPU atlases.
    if (state_.format != 1 || !physical.active() || physical.empty() ||
        state_.names.physical_upload_count > physical.size())
        throw FormatError("next-scene adapters require actual current physical uploads and font atlases");
    const auto difference = std::bit_cast<std::int32_t>(physical.source_cursor() - std::uint32_t{0x6227d4});
    if (state_.published_material_count != std::bit_cast<std::uint32_t>(difference / std::int32_t{28}) ||
        state_.published_material_count != physical.size())
        throw FormatError("next-scene adapters require the actual42E772 physical-count publication");
    backend.require_font_upload_ready(); // same healthy, bound, non-pending backend
    const auto &fonts = platform->state().ui.fonts;
    if (fonts.count == 0 || fonts.count > 2)
        throw FormatError("next-scene fonts have no completed bounded source records");
}
SourceNextSceneAdapterIdentity SessionWorldMaterials::capture_next_scene_adapter_identity(
    const MaterialD3D12 &backend) const
{
    if (source_epoch_phase_ != SourceMaterialEpochPhase::adapters_reserved)
        throw FormatError("only Core's reserved post-tail transaction can capture next-scene adapters");
    require_next_scene_adapter_resources(backend);
    const auto platform = require_registry_live();
    const auto &bank = platform->source_allocations();
    const auto &fonts = platform->state().ui.fonts;
    SourceNextSceneAdapterIdentity identity;
    identity.backend = &backend;
    identity.allocations = &bank;
    identity.scene_epoch = source_epoch_;
    identity.backend_epoch_floor = backend.resource_epoch();
    identity.physical_generation = state_.names.materials.generation();
    identity.font_count = fonts.count;
    for (std::uint32_t i = 0; i < fonts.count; ++i)
    {
        identity.font_scratch[i] = fonts.source_scratch_allocation(i, bank);
        // A full source allocation bank can publish a null scratch at428C63;
        // this scratch is not read by the font loader. Never fabricate one.
        if (identity.font_scratch[i])
            (void)bank.bytes(identity.font_scratch[i]);
        for (unsigned atlas = 0; atlas < 2; ++atlas)
        {
            const auto physical_index = fonts.source_word(i, 0x20u + atlas * 4u);
            const auto token = state_.names.materials[physical_index].primary_texture;
            identity.font_resource_slots[i][atlas] = find_primary_resource(backend, token);
            identity.font_materials[i][atlas] = physical_index;
            identity.font_tokens[i][atlas] = token;
        }
    }
    return identity;
}
void SessionWorldMaterials::require_next_scene_adapter_identity(const SourceNextSceneAdapterIdentity &identity,
                                                                SourceMaterialEpochPhase admitted) const
{
    (void)require_registry_live(); // owner-thread/lifetime check before retained pointer access
    if ((admitted != SourceMaterialEpochPhase::adapters_reserved &&
         admitted != SourceMaterialEpochPhase::adapters_ready) ||
        source_epoch_phase_ != admitted || identity.scene_epoch != source_epoch_ || !identity.backend ||
        loaded_backend_ != identity.backend)
        throw FormatError("next-scene adapter is stale, uncommitted or belongs to another backend");
    const auto &backend = *identity.backend;
    require_next_scene_adapter_resources(backend);
    const auto platform = require_registry_live();
    const auto &bank = platform->source_allocations();
    const auto &fonts = platform->state().ui.fonts;
    if (identity.allocations != &bank || identity.physical_generation != state_.names.materials.generation() ||
        backend.resource_epoch() < identity.backend_epoch_floor || fonts.count != identity.font_count)
        throw FormatError("next-scene adapter retained another allocation, physical or font generation");
    for (std::uint32_t i = 0; i < fonts.count; ++i)
    {
        if (fonts.source_scratch_allocation(i, bank) != identity.font_scratch[i])
            throw FormatError("next-scene font scratch identity was replaced");
        if (identity.font_scratch[i])
            (void)bank.bytes(identity.font_scratch[i]);
        for (unsigned atlas = 0; atlas < 2; ++atlas)
        {
            if (fonts.source_word(i, 0x20u + atlas * 4u) != identity.font_materials[i][atlas] ||
                state_.names.materials[identity.font_materials[i][atlas]].primary_texture !=
                    identity.font_tokens[i][atlas])
                throw FormatError("next-scene font atlas binding changed");
            if (backend.resource_primary_token(identity.font_resource_slots[i][atlas]) !=
                identity.font_tokens[i][atlas])
                throw FormatError("next-scene font GPU resource identity was replaced or released");
        }
    }
}
const WorldMaterialLoadingState &SessionWorldMaterials::next_scene_material_state_for(
    const SourceNextSceneAdapterIdentity &identity, const SceneResources &scene,
    const MaterialTextureBackend &backend) const &
{
    if (scene_ != &scene || identity.backend != &backend)
        throw FormatError("next-scene material borrow has another scene or backend");
    // Ordinary getters cannot use the reserved phase or any loading scope.
    require_next_scene_adapter_identity(identity, SourceMaterialEpochPhase::adapters_ready);
    return state_;
}
PhysicalMaterialStorage &SessionWorldMaterials::next_scene_replacement_storage(
    const SourceNextSceneAdapterIdentity &identity, const MaterialTextureBackend &backend) &
{
    (void)next_scene_material_state_for(identity, *scene_, backend);
    return state_.names.materials;
}
RetailFontResources &SessionWorldMaterials::next_scene_fonts(const SourceNextSceneAdapterIdentity &identity,
                                                             const MaterialTextureBackend &backend) &
{
    (void)next_scene_material_state_for(identity, *scene_, backend);
    return require_registry_live()->state().ui.fonts;
}
const RetailFontResources &SessionWorldMaterials::next_scene_fonts(const SourceNextSceneAdapterIdentity &identity,
                                                                   const MaterialTextureBackend &backend) const &
{
    (void)next_scene_material_state_for(identity, *scene_, backend);
    return require_registry_live()->state().ui.fonts;
}
} // namespace sgn::retail
