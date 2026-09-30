#include "sgn/retail_world_sprite_model.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
namespace
{
void initialize(WorldModelDrawState &state, SourceSpritePacketStorage &sprites, SceneResources *scene)
{
    if (sprites.headers.source_bound() || sprites.source_part.source_bound() ||
        sprites.source_descriptor.source_bound())
        throw FormatError("source sprite allocations require source descriptor initialization");
    // 43409A..4340EF allocate six zeroed buffers through42E8D0. The host owns
    // the typed storage; no emulated executable pointers escape that boundary.
    sprites.headers.fill({});
    sprites.positions.fill({});
    sprites.attributes.fill({});
    // 4340F9 publishes the descriptor before434030 and index/part setup.
    // Typed geometry remains owned by the world when this binding is cleared.
    if (scene)
        scene->bind_sprite_4340f9(state, sprites);
    for (std::size_t i = 0; i < sprites.indices.size(); ++i)
        sprites.indices[i] = static_cast<std::uint16_t>(i); // 434183..434191
    sprites.header_bytes = 0;
    sprites.index_count = 0;
    sprites.position_count = 0;
    sprites.attribute_count = 0;
    sprites.polygons = 0;
    sprites.vertices = 0;
    sprites.corners = 0;
    // 434030 reads freshly allocated zero part+24, not the prior model count.
    state.previous_sprite_polygons = 0;
    state.sprite_part_flags = 1;
    state.sprite_part_flags_02 = 0; // 4341F0/4341FB
    state.sprite_model_flags = 1;
    state.sprite_outer_transform = nullptr; // 434248/43424C
}
} // namespace
void initialize_world_sprite_model(WorldModelDrawState &state, SourceSpritePacketStorage &sprites)
{
    initialize(state, sprites, nullptr);
}
void initialize_world_sprite_model(WorldModelDrawState &state, SourceSpritePacketStorage &sprites,
                                   SceneResources &scene)
{
    scene.require_owner_thread();
    initialize(state, sprites, &scene);
}
} // namespace sgn::retail
