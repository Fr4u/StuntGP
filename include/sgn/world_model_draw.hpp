#pragma once
#include "sgn/retail_model_draw_queue.hpp"
#include "sgn/retail_light_effects.hpp"

namespace sgn::retail
{
// Registry token reserved for source636418's generated sprite model434090.
// It is a live geometry owner, not a PMD and not resource-type1's tyre strip.
ModelTransform world_model_template() noexcept; // initial literal4864E8 only
// Mapped PE initial contents become mutable world storage once.434090 and
// 434030 preserve these globals. Source pointer4788A8 initially names5DA220;
// a nullable native index preserves that alias without self-referential pointers.
struct SpriteSourceStorage
{
    std::array<VehicleVector, 1> normals{{{0, 1, 0, 0}}};                          // 4788B0, fullXYZ/W
    std::array<VehicleVector, 1> bound_vertices{};                                 // 5DA220, distinct from normals
    std::array<std::optional<std::uint32_t>, 1> bound_indices{{std::uint32_t{0}}}; // 4788A8
    std::uint32_t texture_namespace_word{};                                        // 5DA230; no inferred ID semantics
    const VehicleVector &bound_vertex(std::size_t index) const
    {
        const auto target = bound_indices.at(index);
        if (!target)
            throw FormatError("null generated sprite bound binding");
        return bound_vertices.at(*target);
    }
    VehicleVector &bound_vertex(std::size_t index)
    {
        const auto target = bound_indices.at(index);
        if (!target)
            throw FormatError("null generated sprite bound binding");
        return bound_vertices.at(*target);
    }
};
struct WorldModelDrawState
{
    ModelDrawQueue regular, background;
    ModelTransform template_pose{world_model_template()}; // sole mutable4864E8;42B740 writes matrix references here
    // Source4788C0 is the single generated part transform.450150 writes its
    // tail references, including across queue resets.434090 establishes flags.
    ModelTransform sprite_part_transform{{0, 0, 0}, 0, {1, 1, 1}, 0, {0, 0, 1}, 0, {0, 1, 0}, 0, {}};
    SourceOwnedField<std::uint16_t> sprite_part_flags{1}, sprite_model_flags{1};
    SourceOwnedField<std::uint16_t> sprite_part_flags_02{};           // generated part+02,4341FB
    SourceOwnedOuterPointer<ModelTransform> sprite_outer_transform{}; // descriptor+0C, borrowed by42AF30
    std::uint32_t previous_sprite_polygons{};                         // 6249B0,434030 reads old part+24
    SpriteSourceStorage sprite_source;
};
// Full42B740 queue/matrix/sprite-reset stores. Session supplies actual bindings;
// A supplied registry must name these actual owners; standalone leaf oracles
// explicitly omit it and do not qualify a Session registry binding.
void seed_world_model_draws(WorldModelDrawState &, ModelResources &, std::optional<std::size_t> track,
                            std::optional<std::size_t> sky, SourceSpritePacketStorage &, float ground_level,
                            std::uint32_t scene_mode, ViewMatrixArena &, const class SceneResources * = nullptr);
} // namespace sgn::retail
