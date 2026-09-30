#pragma once
#include "sgn/retail_world_material_loading.hpp"
#include <array>

namespace sgn::retail
{
enum class LoadingPrelude
{
    boot,
    loading
};
// Pinned PE483420/483498. Same borrowed entry contract as the actual material
// loader; these tables contain only type0 file textures, with source selector0.
WorldMaterialPrelude source_loading_prelude(LoadingPrelude);
// Four16-byte records4834E0..48351F.445800 adds64 to X when composing the quad.
using LoadingTile = std::array<std::uint32_t, 4>;
std::span<const LoadingTile, 4> source_loading_tiles();
} // namespace sgn::retail
