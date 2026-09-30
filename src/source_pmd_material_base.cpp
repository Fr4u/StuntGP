#include "sgn/source_pmd_material_base.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
namespace
{
std::uint32_t attribute_pointer(const SourcePmdModelView &model, const SourcePmdPartView &part)
{
    // Read the relocated pointer DWORD itself; comparing an empty array's
    // host data() would lose the identity present in the original record.
    return model.bytes().field<std::uint32_t>(part.location().offset + 0x34).value();
}
void read_lod_pointer(const SourcePmdModelView &model, std::uint32_t lod)
{
    if (lod >= 4)
        throw FormatError("material source LOD is outside the four descriptor slots");
    (void)model.descriptor().field<std::uint32_t>(0x48 + lod * 4).value();
}
} // namespace

bool source_pmd_material_part_visited_42b8b0(const SourcePmdModelView &model, std::uint32_t lod, std::uint32_t part)
{
    read_lod_pointer(model, lod); // 42B8B9, even for part0
    if (part == 0)
        return false; // 42B8C4..42B8E5; no current attribute-pointer read
    const auto current = attribute_pointer(model, model.part(lod, part)); // 42B8C8
    for (std::uint32_t previous = 0; previous < part; ++previous)
    {
        const auto earlier = model.part(lod, previous);
        if (current == attribute_pointer(model, earlier) && earlier.attribute_count().value() != 0)
            return true; // 42B8CE..42B8EF; pointer equality AND nonzero WORD count
    }
    return false;
}

void add_source_pmd_material_base_42b900(const SourcePmdModelView &model, const WorldMaterialNamespace &materials)
{
    // INC DWORD wraps: maximumFFFFFFFF is the original empty traversal.
    if (model.maximum_detail_level() + std::uint32_t{1} == 0)
        return;
    std::uint32_t lod = 0;
    do
    {
        read_lod_pointer(model, lod); // 42B926, also reached for zero logical parts
        if (model.part_count() != 0)
        {
            std::uint32_t index = 0;
            do
            {
                const auto part = model.part(lod, index);
                (void)attribute_pointer(model, part); // ESI capture42B93B, before child
                if (!source_pmd_material_part_visited_42b8b0(model, lod, index))
                {
                    const auto count = part.attribute_count().value(); // 42B94E, low16 only
                    if (count != 0)
                    {
                        const auto base = static_cast<std::uint8_t>(materials.logical_base); // DL at42B955
                        for (std::uint32_t polygon = 0; polygon < count; ++polygon)
                        {
                            const auto material = part.attribute_byte(polygon, 1);
                            material.set(static_cast<std::uint8_t>(material.value() + base)); // 42B95E, modulo256
                        }
                    }
                }
                ++index;
            } while (index < model.part_count()); // 42B969..42B975 reread live logical count
        }
        ++lod;
    } while (lod < model.maximum_detail_level() + std::uint32_t{1}); // 42B97D..42B98D
    // No deduplication across LOD slots. Aliased slots are visited again.
}

void bind_source_pmd_material_base_42c563(SceneResources &scene, unsigned slot, const WorldMaterialNamespace &materials)
{
    scene.require_owner_thread();
    const auto handle = scene.model_handle(slot);
    if (!handle)
        throw FormatError("reached material base call has no PMD descriptor");
    add_source_pmd_material_base_42b900(scene.models().source_at(*handle), materials);
}
} // namespace sgn::retail
