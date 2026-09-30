#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include <algorithm>
#include <bit>
namespace sgn::retail
{
bool SessionWorldMaterials::initialize_source_material_limits_42e3e0(std::uint32_t word_477c30)
{
    (void)require_registry_live();
    state_.source_quality_6249a0 = 0;               // 42E3E5
    const bool carry = word_477c30 < 0x3ffff8u;     // 42E3EF unsigned CMP
    state_.source_maximum_dimension_6249a4 = 0x100; // 42E3F4
    return !carry;                                  // 42E3FE: true requires Main42E400..414 root sprintf
}
WorldMaterialLoadingContext SessionWorldMaterials::source_frontend_loading_context(const SourceGraphicsRoots &roots,
                                                                                   WorldMaterialNames transient_names)
{
    const auto platform = require_registry_live();
    auto &world = platform->state();
    if (world.scene_mode != 2)
        throw FormatError("menu material context requires actual scene2");
    if (!source_graphics_roots_borrow_config(roots, world.boot_config))
        throw FormatError("menu material roots must borrow this platform's actual48491C and48499C");
    const auto end = std::find(world.level.track_texture.begin(), world.level.track_texture.end(), '\0');
    if (end == world.level.track_texture.end())
        throw FormatError("unterminated source track prefix");
    WorldMaterialLoadingContext context;
    context.scene = world.scene_mode;
    context.quality = state_.source_quality_6249a0;
    context.maximum_dimension = std::bit_cast<std::int32_t>(state_.source_maximum_dimension_6249a4);
    context.loading_presentation_enabled = world.boot_config.display_boot_graphics; // existing60DFA0
    context.loading_already_shown = state_.source_loading_shown_60f34c;
    context.track_prefix.assign(world.level.track_texture.begin(), end);
    context.transient_image_names = transient_names; // actual441180, no fabricated replacement table
    context.source_graphics_roots = &roots;          // Main owns path transport and root lifetime
    return context;                                  // synchronous argument snapshot, never persistent second owner
}
std::uint32_t &SessionWorldMaterials::source_loading_shown_word() &
{
    (void)require_registry_live();
    return state_.source_loading_shown_60f34c;
}
} // namespace sgn::retail
