#pragma once
#include "sgn/model_resources.hpp"
#include "sgn/retail_world_view_prefix.hpp"

namespace sgn::retail
{
// Native reference encoding in the existing source tail words:0 null,
// 1 shared61C468,2+byte-offset into the sole ViewMatrixArena. Never a pointer.
inline constexpr std::uint32_t shared_model_matrix_reference = 1;
const CameraMatrix &resolve_arena_matrix(const ViewMatrixArena &, std::uint32_t reference);
const CameraMatrix &resolve_model_matrix(const ViewMatrixArena &, const CameraMatrix &shared_identity,
                                         std::uint32_t reference);
// Full450190 publication: pose is ECX, but twist comes from current camera
//[4722D0]+78. Writes camera+50/+4C, two arena matrices, count and cursor.
void allocate_camera_view_matrices(CameraState &pose, ViewMatrixArena &, float current_camera_twist);
// Exact44EE10 writer reached by44F4C0's44F4D4..44F4D9 tail. The shared owner
// must also be republished at that source writer, not merely at process startup.
void initialize_model_matrix_identity(CameraMatrix &) noexcept;
// Outer80B draw pose: forward at20 denotes target; writes only tail[1]/[2].
void allocate_outer_model_matrices(ModelTransform &, ViewMatrixArena &); // full44FDB0
// Actual selected-LOD part order, including repeated transform identities.
void allocate_model_part_matrices(std::span<const ModelPartBinding>, ViewMatrixArena &); // 450150/44FF90
// Descriptor5C source selector uses all four relocated48..54 slots.
void allocate_model_part_matrices(ModelResource &, unsigned source_lod, ViewMatrixArena &);
// Same source part order and arena, writing tail words into the canonical bank.
void allocate_model_part_matrices(SourcePmdModelView, unsigned source_lod, ViewMatrixArena &);
// Source queue path: captured descriptor5C/selected pointer/count, then each
// reached relocated part. No preview LOD lookup or eager scan of later parts.
// Numeric basis uses the existing PC53/round-nearest contract. Publications
// follow44FDB0/44FF90; hardware-FP-trap/intermediate-fault parity is not claimed.
void allocate_source_outer_model_matrices_44fdb0(ModelTransform &, ViewMatrixArena &);
void allocate_source_model_part_matrices_450150(SourcePmdModelView, ViewMatrixArena &);
// Actual static4788C0 transform reached through generated part+08.
void allocate_source_static_part_matrices_44ff90(ModelTransform &, ViewMatrixArena &);
struct VehicleStripTransform;
// Same numeric body and stores on the actual474820 owner, without80-byte marshaling.
void allocate_source_static_part_matrices_44ff90(VehicleStripTransform &, ViewMatrixArena &);
} // namespace sgn::retail
