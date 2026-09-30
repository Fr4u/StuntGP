#include "sgn/scene_resources.hpp"
#include "sgn/retail_light_effects.hpp"

namespace sgn::retail
{
void SceneResources::require_owner_thread() const
{
    if (std::this_thread::get_id() != owner_thread_)
        throw FormatError("scene registry belongs to its creating thread");
}
std::span<const SceneResourceRecord, 16> SceneResources::source_records() const &
{
    require_owner_thread();
    return records_;
}
void SceneResources::bind_sprite_4340f9(WorldModelDrawState &model, SourceSpritePacketStorage &packets)
{
    require_owner_thread();
    if (packets.source_descriptor.source_bound())
    {
        const auto descriptor = packets.source_descriptor.source_binding().allocation;
        if (!descriptor)
        {
            records_[0].descriptor = std::monostate{};
            return;
        }
        records_[0].descriptor = SceneSpriteBinding{&model, &packets, descriptor};
    }
    else
        records_[0].descriptor = SceneSpriteBinding{&model, &packets, {}};
}
void SceneResources::require_sprite_binding(const WorldModelDrawState &model,
                                            const SourceSpritePacketStorage &packets) const
{
    require_owner_thread();
    const auto *binding = std::get_if<SceneSpriteBinding>(&records_[0].descriptor);
    if (!binding || binding->model != &model || binding->packets != &packets)
        throw FormatError("scene sprite binding is null or names another geometry owner");
    if (packets.source_descriptor.source_bound())
    {
        if (binding->source_descriptor != packets.source_descriptor.source_binding().allocation)
            throw FormatError("scene sprite descriptor no longer names the published allocation");
        (void)packets.source_descriptor.view(); // checked lifetime before consuming payload
    }
    else if (binding->source_descriptor)
        throw FormatError("source sprite binding lost its allocation");
}
void SceneResources::bind(VehicleStripGeometry &geometry)
{
    require_owner_thread();
    records_[1].descriptor = &geometry;
}
void SceneResources::bind_source_strip_41c63a(VehicleStripGeometry &geometry, SourceAllocationRef descriptor)
{
    require_owner_thread();
    if (!descriptor)
    {
        records_[1].descriptor = std::monostate{};
        return;
    }
    records_[1].descriptor = SceneStripBinding{&geometry, std::move(descriptor)};
}
const VehicleStripGeometry &SceneResources::strip_geometry() const
{
    require_owner_thread();
    const auto *binding = std::get_if<VehicleStripGeometry *>(&records_[1].descriptor);
    if (!binding || !*binding)
        throw std::logic_error("generated scene geometry is not bound");
    return **binding;
}
LevelModelHandles SceneResources::bind_models(std::span<const LevelAssetRequest> requests,
                                              std::span<const SelectionRecord, 6> selection,
                                              std::span<LevelCarSlot, 6> cars, std::string_view track_set)
{
    require_owner_thread();
    // Existing preview batch semantics. This transient projection is not a
    // second owned registry; the source42F090 path below publishes per store.
    ModelBindingSlots slots{};
    for (unsigned i = 2; i < 16; ++i)
        slots[i] = model_handle(i);
    const auto result = bind_level_model_resources(models_, requests, slots, selection, cars, track_set);
    for (unsigned i = 2; i < 16; ++i)
        if (slots[i])
            records_[i].descriptor = ScenePmdBinding{*slots[i]};
    return result;
}
LevelModelHandles SceneResources::bind_source_models(SourceAllocationBank &bank,
                                                     std::span<const LevelAssetRequest> requests,
                                                     std::span<const SelectionRecord, 6> selection,
                                                     std::span<LevelCarSlot, 6> cars, SourcePmdObserver *observer,
                                                     SourceResourceRegistryObserver *registry_observer)
{
    require_owner_thread();
    for (const auto &request : requests)
    {
        if (request.type < 2 || request.type > 15)
            throw std::out_of_range("unsupported level model binding slot");
        const auto identity = models_.load_source(bank, request.name, observer); // 42F0DE ->42EFB0
        const auto handle = *models_.resolve(identity);
        const auto &loading = models_.source_loading_state();
        const auto slot = static_cast<unsigned>(request.type);
        auto &row = records_[slot];
        row.descriptor = ScenePmdBinding{handle}; // 42F0F8
        if (registry_observer)
            registry_observer->published({0x42f0f8, 0x636418 + slot * 16, slot, 0}, *this);
        row.file = loading.file; // 42F104, no payload borrow
        if (registry_observer)
            registry_observer->published({0x42f104, 0x636420 + slot * 16, slot, 8}, *this);
        row.file_size = loading.file_size; // 42F10A
        if (registry_observer)
            registry_observer->published({0x42f10a, 0x636424 + slot * 16, slot, 12}, *this);
    }
    // 42F116..42F15A follows all requests. Failure in a later request keeps
    // earlier successful registry publications; it has not reached this loop.
    unsigned compact = 0;
    for (unsigned physical = 0; physical < 6; ++physical)
        if (selection[physical].kind == 2)
        {
            cars[physical].source_word00 = models_.identity(model_handle(4 + compact * 2));
            cars[physical].source_word04 = models_.identity(model_handle(5 + compact * 2));
            ++compact;
        }
    return {model_handle(2), model_handle(3)};
}
std::optional<std::size_t> SceneResources::model_handle(unsigned slot) const
{
    require_owner_thread();
    if (slot < 2 || slot >= records_.size())
        throw std::out_of_range("PMD binding slot");
    const auto *binding = std::get_if<ScenePmdBinding>(&records_[slot].descriptor);
    return binding ? std::optional(binding->handle) : std::nullopt;
}
} // namespace sgn::retail
