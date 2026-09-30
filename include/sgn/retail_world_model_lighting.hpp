#pragma once
#include "sgn/retail_light_effects.hpp"
#include "sgn/retail_color_tables.hpp"

namespace sgn::retail
{
// Borrowed source transform/normal-camera bindings. Matrix identities are
// resolved by the caller from its existing arena; these are not new owners.
struct WorldModelLightingPose
{
    const VehicleVector &normalized_camera; // 61C4F8
    Vec3 outer_position, part_position;
    const CameraMatrix &inverse_outer; // descriptor0C -> transform48
    const CameraMatrix *inverse_part;  // part08 -> transform44, used for >1 part
    std::uint32_t part_count;
};
struct WorldModelLightingPart
{
    std::uint16_t model_ambience, part_flags_02;
    std::span<std::array<std::uint32_t, 4>> coordinates; // same mutable PMD owner
    std::span<const VehicleVector> vertices, normals;
};
// Full42B400 including44EFD0. LightVertexCursor borrows existing624530/624574.
// Record+20 is bank index+1, resolving into the same PmdLightBank. The source
// pointer cannot be stored as a truncated native address. Retain +0C/+1C bytes.
void prepare_world_model_lights(const WorldModelLightingPose &, PmdLightBank &, const LightViewSelection &,
                                LightVertexCursor &);
// Full42A320/42A570, each includes prepare_world_model_lights. Stride62497C and
// stamp61C508 select actual retained coordinate records, not simulation ticks.
void shade_world_model_diffuse(const WorldModelLightingPose &, WorldModelLightingPart, PmdLightBank &,
                               const LightViewSelection &, LightVertexCursor &, const RetailAmbientLight &,
                               std::uint32_t frame_stamp, std::uint32_t stride);
void shade_world_model_specular(const WorldModelLightingPose &, WorldModelLightingPart, PmdLightBank &,
                                const LightViewSelection &, LightVertexCursor &, const RetailAmbientLight &,
                                const ColorTables &, std::uint32_t frame_stamp, std::uint32_t stride);
void select_world_model_lighting_stride(std::uint32_t &stride, std::uint32_t quality, std::uint32_t current_lod,
                                        std::uint32_t previous_lod) noexcept; // full42A840
void select_world_part_lighting_stride(std::uint32_t &stride, std::uint32_t quality, std::uint32_t current_lod,
                                       std::uint32_t previous_lod, std::uint16_t part_word_04) noexcept; // full42A870
} // namespace sgn::retail
