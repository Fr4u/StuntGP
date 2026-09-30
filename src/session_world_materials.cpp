#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/resource_material_registry.hpp"
#include "sgn/retail_material_filesystem.hpp"
#include <algorithm>

namespace sgn::retail
{
SessionWorldMaterials::SessionWorldMaterials(RaceSession &session)
    : platform_(session.shared_platform()), owner_thread_(std::this_thread::get_id()),
      ticket_(capture_world_polygon_level(session.state().polygon_frame)), scene_(&session.scene_resources())
{
    const auto platform = require_live();
    state_.effect_animation_storage = &platform->state().effect_resources.animations;
    state_.names.materials.reset_scene_42e510();
}
SessionWorldMaterials::SessionWorldMaterials(SourceColdMaterialAttachmentOnly,
                                             std::shared_ptr<RaceSessionPlatform> platform)
    : platform_(platform), owner_thread_(std::this_thread::get_id()), source_registry_only_(true)
{
    if (!platform)
        throw FormatError("source materials require their platform owner");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw FormatError("source materials require source construction entry");
    if (platform->source_material_owner_claimed_)
        throw FormatError("source material owner already claimed for this world");
    // Reservation survives even a later construction failure or destruction.
    platform->source_material_owner_claimed_ = true;
    scene_ = &platform->source_scene_resources();
    state_.effect_animation_storage = &platform->state().effect_resources.animations;
}
std::shared_ptr<RaceSessionPlatform> SessionWorldMaterials::require_registry_live() const
{
    if (!source_registry_only_)
        return require_live(); // unchanged preview tickets/guards
    if (std::this_thread::get_id() != owner_thread_)
        throw FormatError("Session materials belong to their creating thread");
    auto platform = platform_.lock();
    if (!platform)
        throw FormatError("Session material world has expired");
    platform->require_owner_thread();
    if (scene_ != &platform->source_scene_resources())
        throw FormatError("source material registry owner changed");
    return platform;
}
std::uint64_t SessionWorldMaterials::source_resource_epoch() const
{
    (void)require_registry_live();
    return source_epoch_;
}
SourceMaterialEpochPhase SessionWorldMaterials::source_resource_epoch_phase() const
{
    (void)require_registry_live();
    return source_epoch_phase_;
}
void SessionWorldMaterials::require_source_cold_resource_epoch() const
{
    (void)require_registry_live();
    if (source_epoch_phase_ != SourceMaterialEpochPhase::cold)
        throw FormatError("source material borrow belongs to a retired or unfinished resource epoch");
}
void SessionWorldMaterials::require_source_resource_epoch(std::uint64_t epoch) const
{
    require_source_cold_resource_epoch();
    if (epoch != source_epoch_)
        throw FormatError("source material adapter retained another resource epoch");
}
SessionWorldMaterials::~SessionWorldMaterials()
{
    // Clearing pointer containers never dereferences expired model names.
    // No source release/reset is synthesized from a host destructor.
    state_.names.logical_names.clear();
    state_.names.unique_names.clear();
}
std::shared_ptr<RaceSessionPlatform> SessionWorldMaterials::require_live() const
{
    if (source_registry_only_)
        throw FormatError("source registry attachment does not admit material loading or rendering");
    if (std::this_thread::get_id() != owner_thread_)
        throw FormatError("Session materials belong to their creating thread");
    auto platform = platform_.lock();
    if (!platform)
        throw FormatError("Session material world has expired");
    platform->require_owner_thread();
    validate_world_polygon_level(platform->state().polygon_frame, ticket_);
    if (failed_)
        throw FormatError("Session material loading failed; this world must be discarded");
    if (loaded_)
        for (unsigned slot = 2; slot < 16; ++slot)
            if (scene_->model_handle(slot) != loaded_models_[slot - 2])
                throw FormatError("Session material model bindings have changed");
    return platform;
}
const WorldMaterialLoadingState &SessionWorldMaterials::state() const &
{
    (void)require_live();
    return state_;
}
const WorldMaterialLoadingState &SessionWorldMaterials::validate_for(const RaceSession &session) const &
{
    const auto platform = require_live();
    if (platform != session.shared_platform())
        throw FormatError("Session materials belong to a different world");
    validate_world_polygon_level(session.state().polygon_frame, ticket_);
    if (!loaded_ || !published_ || state_.names.materials.empty() ||
        state_.published_material_count != state_.names.materials.size() ||
        state_.names.physical_upload_count > state_.names.materials.size())
        throw FormatError("Session materials require completed loading and source42E772 publication");
    return state_;
}
void SessionWorldMaterials::require_backend_for(const RaceSession &session, const MaterialTextureBackend &backend) const
{
    (void)validate_for(session);
    if (loaded_backend_ != &backend)
        throw FormatError("Session material release has another loading backend");
}
std::span<std::uint32_t, 16> SessionWorldMaterials::registry_flags() &
{
    (void)require_registry_live();
    return registry_flags_;
}
std::span<const std::uint32_t, 16> SessionWorldMaterials::registry_flags_for(const SceneResources &scene) const &
{
    const auto platform = require_registry_live(); // holds world through preflight
    if (scene_ != &scene)
        throw FormatError("material flags belong to a different scene registry");
    return registry_flags_;
}
PhysicalMaterialStorage &SessionWorldMaterials::replacement_storage(const RaceSession &session) &
{
    (void)validate_for(session);
    return state_.names.materials;
}
void SessionWorldMaterials::load(WorldMaterialLoadingContext &context, MaterialTextureBackend &backend,
                                 WorldMaterialLoadingPresentation *presentation)
{
    const auto platform = require_live();
    if (attempted_ || platform->cold_materials_claimed_)
        throw FormatError("Session materials require fresh model headers; repeated cold loading is unsupported");
    if (context.scene != platform->state().scene_mode)
        throw FormatError("Session material scene does not match its world");
    const auto &prefix = platform->state().level.track_texture;
    const auto end = std::find(prefix.begin(), prefix.end(), '\0');
    if (end == prefix.end() || context.track_prefix != std::string(prefix.begin(), end))
        throw FormatError("Session material prefix does not match its loaded level");
    std::array<ResourceMaterialBinding, 14> bindings{};
    std::array<std::optional<std::size_t>, 14> handles;
    for (unsigned slot = 2; slot < 16; ++slot)
    {
        const auto handle = scene_->model_handle(slot);
        handles[slot - 2] = handle;
        bindings[slot - 2] = {handle ? &scene_->models().at(*handle) : nullptr, registry_flags_[slot]};
    }
    ResourceMaterialRegistry registry(bindings);
    RetailMaterialFileSystem files(platform->assets(), platform->retail_root());
    platform->cold_materials_claimed_ = true;
    attempted_ = true;
    try
    {
        load_world_materials(registry.view(), state_, context, files, backend, presentation);
        loaded_models_ = handles;
        loaded_backend_ = &backend;
        loaded_ = true;
    }
    catch (...)
    {
        failed_ = true;
        detach_world_polygon_level(platform->state().polygon_frame);
        throw;
    }
}
void SessionWorldMaterials::publish_count_42e772()
{
    (void)require_live();
    if (!loaded_ || state_.names.physical_upload_count > state_.names.materials.size() ||
        state_.names.materials.size() > 255)
        throw FormatError("Session material physical table is not a completed source owner");
    // 42E753..42E772 publishes(physical_pointer -6227D4)/28. With this
    // bounded typed table the result is exactly its number of actual records.
    // 58271C counts the ordinary upload path; font4325E0 does not advance it.
    state_.published_material_count = static_cast<std::uint32_t>(state_.names.materials.size());
    published_ = true;
}
} // namespace sgn::retail
