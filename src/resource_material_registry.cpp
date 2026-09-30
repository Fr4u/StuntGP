#include "sgn/resource_material_registry.hpp"
#include <algorithm>
#include <deque>
#include <limits>

namespace sgn::retail
{
struct ResourceMaterialRegistry::Storage
{
    struct ModelView
    {
        ModelResource *owner;
        std::vector<std::vector<WorldMaterialPartView>> parts;
        std::vector<WorldMaterialLodView> lods;
        std::vector<const WorldMaterialLodView *> slots;
        WorldMaterialModelView view;
        explicit ModelView(ModelResource &model) : owner(&model)
        {
            const auto count = model.lod_count();
            if (count == 0 || count > 4)
                throw FormatError("material registry model LOD domain");
            const auto part_count = model.part_count(model.source_lod_index(0));
            if (part_count > std::numeric_limits<std::uint32_t>::max())
                throw FormatError("material registry part count overflow");
            parts.resize(count);
            lods.resize(count);
            slots.reserve(count);
            // Storage by canonical LOD; the source slot list below retains
            // repeated pointers instead of deduplicating visits across slots.
            for (unsigned source = 0; source < count; ++source)
            {
                const auto canonical = model.source_lod_index(source);
                if (canonical >= count || model.part_count(canonical) != part_count)
                    throw FormatError("material registry source LOD/part shape");
                if (lods[canonical].parts.data() == nullptr)
                {
                    auto &list = parts[canonical];
                    list.reserve(part_count);
                    for (unsigned part = 0; part < part_count; ++part)
                        list.push_back({model.source_attributes(canonical, part)});
                    lods[canonical].parts = list;
                }
                slots.push_back(&lods[canonical]);
            }
            view = {slots, static_cast<std::uint32_t>(part_count), model.raw_source_texture_names()};
        }
    };
    std::deque<ModelView> models;
    std::vector<WorldMaterialRegistryEntry> entries;
    explicit Storage(std::span<const ResourceMaterialBinding> bindings)
    {
        // Only this metadata allocation is bounded. The original walk ends
        // at its first null; entries after that null are not dereferenced.
        constexpr std::size_t host_registry_limit = 4096;
        const auto terminal = std::find_if(bindings.begin(), bindings.end(),
                                           [](const auto &binding) { return binding.model == nullptr; });
        const auto count = static_cast<std::size_t>(terminal - bindings.begin());
        if (count > host_registry_limit)
            throw FormatError("material registry host entry bound");
        entries.reserve(count + 1);
        for (const auto &binding : bindings.first(count))
        {
            auto found = std::find_if(models.begin(), models.end(),
                                      [&](const auto &model) { return model.owner == binding.model; });
            if (found == models.end())
            {
                models.emplace_back(*binding.model);
                found = std::prev(models.end());
            }
            entries.push_back({&found->view, binding.flags04});
        }
        entries.push_back({nullptr, terminal == bindings.end() ? 0u : terminal->flags04});
    }
};
ResourceMaterialRegistry::ResourceMaterialRegistry(std::span<const ResourceMaterialBinding> bindings)
    : storage_(std::make_unique<Storage>(bindings))
{
}
ResourceMaterialRegistry::~ResourceMaterialRegistry() = default;
WorldMaterialRegistry ResourceMaterialRegistry::view() const & noexcept
{
    return storage_->entries;
}
} // namespace sgn::retail
