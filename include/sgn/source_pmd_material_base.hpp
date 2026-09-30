#pragma once
#include "sgn/source_pmd_model.hpp"
#include "sgn/retail_world_materials.hpp"

namespace sgn::retail
{
class SceneResources;
// Same PMD allocation, no decoded attribute copies. Source42B8B0 compares
// part+34 pointer values, then the preceding part's WORD+24 count.
bool source_pmd_material_part_visited_42b8b0(const SourcePmdModelView &, std::uint32_t source_lod, std::uint32_t part);
void add_source_pmd_material_base_42b900(const SourcePmdModelView &, const WorldMaterialNamespace &);
// Reached CALL42C563, after42C440 has loaded this model's names and established
// 582724 in the existing namespace. Does not perform or pretend that load.
void bind_source_pmd_material_base_42c563(SceneResources &, unsigned registry_slot, const WorldMaterialNamespace &);
} // namespace sgn::retail
