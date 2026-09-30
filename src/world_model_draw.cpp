#include "sgn/world_model_draw.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/source_world_draw_seeding.hpp"

namespace sgn::retail
{
ModelTransform world_model_template() noexcept
{
    return {{0, 0, 0}, 0, {1, 1, 1}, 0, {0, 0, -1}, 0, {0, 1, 0}, 0, {0, 0, 0, 2}};
}
void seed_world_model_draws(WorldModelDrawState &state, ModelResources &models, std::optional<std::size_t> track,
                            std::optional<std::size_t> sky, SourceSpritePacketStorage &sprites, float ground,
                            std::uint32_t scene, ViewMatrixArena &matrices, const SceneResources *registry)
{
    if (scene != 2 && sprites.source_part.source_bound())
    {
        if (!registry)
            throw SourceAllocationError(0x42b740, "source queue seeding requires the actual scene registry");
        seed_source_world_model_draws_42b740(state, models, sprites, ground, scene, matrices, *registry);
        return;
    }
    if (registry && scene != 2)
        registry->require_sprite_binding(state, sprites);     // host preflight before mutations
    reset_model_draw_queues(state.regular, state.background); // 42B745..42B766
    if (scene == 2)
        return; // 42B74F/42B770
    if (track)
    {
        auto &pose = state.template_pose;
        append_model_draw(state.background, models, *track, 0, pose, 0, matrices);
    } // 42B783
    if (sky)
    {
        auto pose = state.template_pose;
        pose.position.y = ground;
        pose.forward.x = vehicle_f32(double(pose.forward.x) + pose.position.x);
        pose.forward.y = vehicle_f32(double(pose.forward.y) + pose.position.y);
        pose.forward.z = vehicle_f32(double(pose.forward.z) + pose.position.z);
        append_model_draw(state.background, models, *sky, 0, pose, 0, matrices);
    } // 42B6E0
    // 434030 resets working pointers/counters, retaining allocation bytes.
    state.previous_sprite_polygons = sprites.polygons;
    sprites.header_bytes = 0;
    sprites.index_count = 0;
    sprites.position_count = 0;
    sprites.attribute_count = 0;
    sprites.polygons = 0;
    sprites.vertices = 0;
    sprites.corners = 0;
    // 42B7A3: source636418 is434090's one-part model with all sourceLOD slots
    // aliasing its single part. Keep its transform owner and exact allocator.
    auto &queue = state.background;
    if (!queue.cursor || *queue.cursor >= queue.records.size() - 1)
        throw FormatError("generated model draw queue capacity");
    auto &pose = state.template_pose;
    allocate_outer_model_matrices(pose, matrices);
    auto part_flags = static_cast<std::uint16_t>(state.sprite_part_flags);
    const Vec3 pivot{};
    const std::array<ModelPartBinding, 1> parts{{{part_flags, state.sprite_part_transform, pivot}}};
    allocate_model_part_matrices(parts, matrices);
    state.sprite_part_flags = part_flags;
    pose.tail[0] = 0;
    queue.records[*queue.cursor] = {sprite_model_identity, pose};
    auto sprite_flags = static_cast<std::uint16_t>(state.sprite_model_flags);
    clear_model_draw_flags(sprite_flags);
    state.sprite_model_flags = sprite_flags;
    ++*queue.cursor;
    queue.records[*queue.cursor].model_identity = 0;
}
} // namespace sgn::retail
