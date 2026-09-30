#pragma once
#include "sgn/retail_model_matrices.hpp"

namespace sgn::retail
{
struct ModelDrawCommand
{
    std::uint32_t model_identity{}; // native registry token;zero terminates,FFFFFFFF is generated sprite model
    ModelTransform pose;
};
static_assert(sizeof(ModelDrawCommand) == 84);
struct ModelDrawQueue
{
    // 62F5D8..632CF7 and632CF8..636417 are168records each. One record's
    // identity must remain available for the zero terminator after an append.
    std::array<ModelDrawCommand, 168> records{};
    std::optional<std::uint32_t> cursor; // source4955E0/4955E4 as a record index, not a second count
};
// 42B745..42B766 only. Remaining42B740 static-world enqueues are separate.
void reset_model_draw_queues(ModelDrawQueue &regular, ModelDrawQueue &background) noexcept;
struct ModelDrawPartFlags
{
    const std::uint16_t &visibility;
    std::uint16_t &flags;
};
void force_model_draw_flags(std::uint16_t &model_flags, std::span<const ModelDrawPartFlags>,
                            std::uint32_t flags);                 // full42B7B0, including zero parts
void clear_model_draw_flags(std::uint16_t &model_flags) noexcept; // full42B7E0
// 42B850 and42B7F0 have identical bodies with different queue owners. The
// caller supplies the selected sourceLOD5C and that model's registry identity.
void append_model_draw(ModelDrawQueue &, SourcePmdModelView, unsigned source_lod, std::uint32_t model_identity,
                       ModelTransform &, std::uint32_t flags, ViewMatrixArena &);
// Registry dispatch uses one model owner; it never decodes a legacy mirror.
void append_model_draw(ModelDrawQueue &, ModelResources &, std::size_t handle, unsigned source_lod, ModelTransform &,
                       std::uint32_t flags, ViewMatrixArena &);
void append_model_draw(ModelDrawQueue &, ModelResource &, unsigned source_lod, std::uint32_t model_identity,
                       ModelTransform &outer_pose, std::uint32_t flags, ViewMatrixArena &);
// Full42B7F0 with live descriptor5C and captured raw part traversal450150.
// Source values/parts stay in the actual PMD; the queue owns only the original
// copied80-byte pose. Reached earlier matrix/pose writes survive later guards.
void append_source_model_draw_42b7f0(ModelDrawQueue &, SourcePmdModelView, std::uint32_t model_identity,
                                     ModelTransform &, std::uint32_t flags, ViewMatrixArena &);
} // namespace sgn::retail
