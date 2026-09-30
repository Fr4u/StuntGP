#pragma once
#include "sgn/model_resources.hpp"
#include "sgn/retail_vehicle_strip.hpp"
#include "sgn/source_scene_resource_prefix.hpp"
#include <thread>
#include <variant>

namespace sgn::retail
{
struct WorldModelDrawState;
struct SourceSpritePacketStorage;
struct SceneSpriteBinding
{
    WorldModelDrawState *model;
    SourceSpritePacketStorage *packets;
    SourceAllocationRef source_descriptor; // retained allocation identity at4340F9; null in native path
    bool operator==(const SceneSpriteBinding &) const = default;
};
struct ScenePmdBinding
{
    std::size_t handle;
    bool operator==(const ScenePmdBinding &) const = default;
};
struct SceneStripBinding
{
    VehicleStripGeometry *owner{};
    SourceAllocationRef source_descriptor; // captured41C63A; independent of current571314
    bool operator==(const SceneStripBinding &) const = default;
};
// One canonical source row636418+i*16. Native-width typed identities replace
// source pointers; do not reinterpret this layout as16 x86 bytes. Flags+4
// stay exclusively in the same world's SessionWorldMaterials owner.
struct SceneResourceRecord
{
    std::variant<std::monostate, SceneSpriteBinding, VehicleStripGeometry *, ScenePmdBinding, SceneStripBinding>
        descriptor;
    SourceAllocationRef file;  //+8:42F104; retained even after payload release
    std::uint32_t file_size{}; //+0C:42F10A; never derived from expired storage
    bool operator==(const SceneResourceRecord &) const = default;
};
class SceneResources final : public VehicleStripBinding
{
    ModelResources models_;
    std::array<SceneResourceRecord, 16> records_{};
    // Source DWORD636518 after the sixteen records: PE zero-filled .data.
    // 42E510 clears through636514 and retains this independent terminal word.
    std::uint32_t material_terminal_636518_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    friend SourceSceneResourcePrefixResult reset_source_scene_resource_prefix_42e510(SceneResources &,
                                                                                     SessionWorldMaterials &,
                                                                                     SourceResourceRegistryObserver *);

  public:
    SceneResources(const AssetStore &assets, std::filesystem::path retail_root)
        : models_(assets, std::move(retail_root))
    {
    }
    SceneResources(const SceneResources &) = delete;
    SceneResources &operator=(const SceneResources &) = delete;
    SceneResources(SceneResources &&) = delete;
    SceneResources &operator=(SceneResources &&) = delete;
    void require_owner_thread() const;
    std::uint32_t material_registry_terminal_636518() const
    {
        require_owner_thread();
        return material_terminal_636518_;
    }
    ModelResources &models() noexcept
    {
        return models_;
    }
    const ModelResources &models() const noexcept
    {
        return models_;
    }
    // Metadata-only synchronous borrow; does not dereference source payloads.
    std::span<const SceneResourceRecord, 16> source_records() const &;
    std::span<const SceneResourceRecord, 16> source_records() const && = delete;
    void bind_sprite_4340f9(WorldModelDrawState &, SourceSpritePacketStorage &);
    void require_sprite_binding(const WorldModelDrawState &, const SourceSpritePacketStorage &) const;
    void bind(VehicleStripGeometry &) override; // type1, published before expansion
    void bind_source_strip_41c63a(VehicleStripGeometry &, SourceAllocationRef);
    const VehicleStripGeometry &strip_geometry() const;
    LevelModelHandles bind_models(std::span<const LevelAssetRequest>, std::span<const SelectionRecord, 6>,
                                  std::span<LevelCarSlot, 6>, std::string_view track_set = {});
    LevelModelHandles bind_source_models(SourceAllocationBank &, std::span<const LevelAssetRequest>,
                                         std::span<const SelectionRecord, 6>, std::span<LevelCarSlot, 6>,
                                         SourcePmdObserver * = nullptr, SourceResourceRegistryObserver * = nullptr);
    // Full42F090, called at42E6F5 after the preceding request producers.
    // Consumes the live sentinel list, not a snapshot or a new request vector.
    void load_source_requests_42f090(SourceAllocationBank &, std::uint32_t scene_mode,
                                     std::span<const SelectionRecord, 6>, std::span<LevelCarSlot, 6>,
                                     SourcePmdObserver * = nullptr, SourceResourceRegistryObserver * = nullptr);
    void load_source_requests_42f090(SourceAllocationBank &, SourcePmdFileSource &, std::uint32_t scene_mode,
                                     std::span<const SelectionRecord, 6>, std::span<LevelCarSlot, 6>,
                                     SourcePmdObserver * = nullptr, SourceResourceRegistryObserver * = nullptr);
    std::optional<std::size_t> model_handle(unsigned binding_slot) const;
};
} // namespace sgn::retail
