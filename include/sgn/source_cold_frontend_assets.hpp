#pragma once
#include "sgn/source_cold_scene_resources.hpp"
#include "sgn/retail_world_material_loading.hpp"
namespace sgn
{
class MaterialD3D12;
}
namespace sgn::retail
{
struct FontBankObserver;

// Reached slice only. Requires successful earlier renderer431360/431300 on
// the SAME cold world; it cannot initialize a device or alter6244F8 to get here.
// 441180's actual App-owned terminal is borrowed internally; material/renderer/presentation live
// through the whole call. Required445A40 still follows the actual source flags.
// Completes429510,42E3E0,425070,413190/413220,42C510,44B1A0,428FA0,42E772.
// The next source instruction42E778 is still required for full42E6E0.
struct SourceColdFrontendAssetsResult
{
    std::uint32_t next_instruction{0x42e778};
};
[[nodiscard]] SourceColdFrontendAssetsResult load_source_cold_frontend_assets_42e72b(
    SourceColdSceneResourceContext &, MaterialD3D12 &, WorldMaterialLoadingPresentation * = nullptr,
    FontBankObserver * = nullptr);
} // namespace sgn::retail
