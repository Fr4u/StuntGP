#pragma once
#include "sgn/retail_world_material_loading.hpp"
#include <filesystem>
#include <optional>

namespace sgn::retail
{
// New host boundary for original44B3B0/44B260 routing. The AssetStore retains
// all archive payloads; this view owns no copied WAD or game-world state.
// The supplied root contains loose graphics directories and is independent
// of the retail installation path and the import ZIP location.
class RetailMaterialFileSystem final : public MaterialFileSource
{
    const AssetStore &assets_;
    std::filesystem::path root_;
    std::string key(const MaterialResourcePath &) const;
    const Archive *routed_archive(std::string_view, bool exact) const;
    std::optional<std::span<const std::uint8_t>> archived(const Archive &, std::string_view) const;
    std::filesystem::path loose_path(std::string_view) const;

  public:
    RetailMaterialFileSystem(const AssetStore &, std::filesystem::path data_root);
    RetailMaterialFileSystem(AssetStore &&, std::filesystem::path) = delete;
    RetailMaterialFileSystem(const AssetStore &&, std::filesystem::path) = delete;
    std::uint32_t source_file_size(const MaterialResourcePath &) override;
    MaterialFileRead read_source_file(const MaterialResourcePath &, std::span<std::uint8_t>) override;
};
} // namespace sgn::retail
