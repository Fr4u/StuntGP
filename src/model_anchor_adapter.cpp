#include "sgn/model_anchor_adapter.hpp"
#include <bit>
#include <limits>

namespace sgn::retail
{
ModelAnchorAdapter::ModelAnchorAdapter(ModelResource &resource) : resource_(std::ref(resource)) {}
ModelAnchorAdapter::ModelAnchorAdapter(SourcePmdModelView resource) : resource_(resource)
{
    resource.check();
}
ModelAnchorAdapter::ModelAnchorAdapter(ModelResource &resource, unsigned selected_lod)
    : resource_(std::ref(resource)), lod_(selected_lod)
{
    if (resource.part_count(*lod_) > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("model part count outside source range");
}
ModelAnchorAdapter::ModelAnchorAdapter(SourcePmdModelView resource, unsigned selected_lod)
    : resource_(resource), lod_(selected_lod)
{
    if (selected_lod > resource.maximum_detail_level())
        throw FormatError("source PMD anchor LOD");
    (void)resource.part(selected_lod, 0);
}
unsigned ModelAnchorAdapter::selected_lod() const
{
    if (const auto *source = std::get_if<SourcePmdModelView>(&resource_))
    {
        source->check();
        return lod_ ? *lod_ : source->current_detail_level().value();
    }
    const auto &model = std::get<std::reference_wrapper<ModelResource>>(resource_).get();
    return lod_ ? *lod_ : model.current_detail_level();
}
std::uint32_t ModelAnchorAdapter::part_count() const
{
    if (const auto *source = std::get_if<SourcePmdModelView>(&resource_))
        return source->part_count();
    const auto &model = std::get<std::reference_wrapper<ModelResource>>(resource_).get();
    return static_cast<std::uint32_t>(
        model.part_count(lod_ ? *lod_ : model.source_lod_index(model.current_detail_level())));
}
std::uint16_t ModelAnchorAdapter::part_flags(unsigned index, unsigned lod) const
{
    if (const auto *source = std::get_if<SourcePmdModelView>(&resource_))
        return source->part(lod_ ? lod : source->source_lod_index(lod), index).flags().value();
    auto &model = std::get<std::reference_wrapper<ModelResource>>(resource_).get();
    return model.source_part(lod_ ? lod : model.source_lod_index(lod), index).flags;
}
VehicleAnchorPart ModelAnchorAdapter::part(unsigned index, unsigned lod) const
{
    if (const auto *source = std::get_if<SourcePmdModelView>(&resource_))
    {
        const auto p = source->part(lod_ ? lod : source->source_lod_index(lod), index);
        const auto t = p.transform().value();
        return {p.flags().value(), t.position, t.scale, t.forward};
    }
    auto &model = std::get<std::reference_wrapper<ModelResource>>(resource_).get();
    const auto p = model.source_part(lod_ ? lod : model.source_lod_index(lod), index);
    const auto &t = p.transform;
    const auto v = [](Vec3 xyz, std::uint32_t w) {
        return VehicleVector{xyz.x, xyz.y, xyz.z, std::bit_cast<float>(w)};
    };
    return {p.flags, v(t.position, t.position_w), v(t.scale, t.scale_w), v(t.forward, t.forward_w)};
}
float ModelAnchorAdapter::maximum_vertex_z(unsigned index, unsigned lod) const
{
    if (const auto *source = std::get_if<SourcePmdModelView>(&resource_))
        return source->part(lod_ ? lod : source->source_lod_index(lod), index).maximum_vertex_z();
    auto &model = std::get<std::reference_wrapper<ModelResource>>(resource_).get();
    return model.maximum_source_vertex_z(lod_ ? lod : model.source_lod_index(lod), index);
}
} // namespace sgn::retail
