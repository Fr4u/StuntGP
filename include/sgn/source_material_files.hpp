#pragma once
#include "sgn/source_boot_config.hpp"
#include "sgn/retail_world_material_loading.hpp"

namespace sgn::retail
{
// Typed path transport into the existing44B3B0/44B260 and its single WAD cache.
// All owners are borrowed for a synchronous load. No AssetStore fallback/cache.
class SourceMaterialFiles final : public MaterialFileSource
{
    SourceBootConfigState &config_;
    SourceBootFilePlatform &files_;
    const SourceGraphicsRoots &roots_;
    std::string source_path(const MaterialResourcePath &) const;

  public:
    SourceMaterialFiles(SourceBootConfigState &config, SourceBootFilePlatform &files, const SourceGraphicsRoots &roots)
        : config_(config), files_(files), roots_(roots)
    {
    }
    std::uint32_t source_file_size(const MaterialResourcePath &) override;
    MaterialFileRead read_source_file(const MaterialResourcePath &, std::span<std::uint8_t>) override;
};
} // namespace sgn::retail
