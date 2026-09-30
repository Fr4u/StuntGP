#pragma once
#include "sgn/retail_world_material_loading.hpp"

namespace sgn::retail
{
class SceneResources;
void replace_source_pmd_material_id_42ba00(SceneResources &, std::uint32_t from, std::uint32_t to);
void remap_source_pmd_material_names_42ba90(SceneResources &, const WorldMaterialNamespace &);
// Reached42C510 consumer. Caller supplies the SAME world's registry flags,
// loading state and backend; this function does not create a replacement owner
// or authorize SessionWorldMaterials' still-incomplete cold source startup.
void load_source_pmd_world_materials_42c510(SceneResources &, std::span<const std::uint32_t, 16> flags,
                                            WorldMaterialLoadingState &, WorldMaterialLoadingContext &,
                                            MaterialFileSource &, MaterialTextureBackend &,
                                            WorldMaterialLoadingPresentation * = nullptr);
} // namespace sgn::retail
