#pragma once
#include "sgn/source_allocation_bank.hpp"
namespace sgn
{
struct MaterialTransparencyStorage;
}
namespace sgn::retail
{
class RaceSessionPlatform;
// 4310A0 on the actual two-coordinate record. Finite binary32 coordinates;
// reached nonfinite values stop before their unavailable CRT error branch.
// An error on V retains the completed U stores. No coordinate copy is edited.
void fix_source_uv_coordinates_4310a0(SourceAllocationBank &, SourceAddressLocation);
// Full4312C0/4311C0 traversal for the actual source PMD and generated-sprite
// owners. Caller supplies this world's existing transparency59207C bank.
// Does not activate warm42E6E0; native preview/strip descriptors are rejected.
// Stable owners on their owning thread, no callback or retained payload view.
void fix_source_scene_uv_4312c0(RaceSessionPlatform &, const MaterialTransparencyStorage &);
} // namespace sgn::retail
