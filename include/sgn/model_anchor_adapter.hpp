#pragma once
#include "sgn/model_resources.hpp"
#include "sgn/retail_vehicle_anchors.hpp"

namespace sgn::retail
{
// The initializer sees the same live flags/transforms as presentation. This
// adapter owns no model state and its borrowed resource must outlive it.
class ModelAnchorAdapter final : public VehicleAnchorModel
{
    std::variant<std::reference_wrapper<ModelResource>, SourcePmdModelView> resource_;
    std::optional<unsigned> lod_; // explicit resident for older receiving; absent means current raw source slots
  public:
    explicit ModelAnchorAdapter(ModelResource &);
    explicit ModelAnchorAdapter(SourcePmdModelView);
    ModelAnchorAdapter(ModelResource &, unsigned selected_lod);
    ModelAnchorAdapter(SourcePmdModelView, unsigned selected_lod);
    std::uint32_t part_count() const override;
    unsigned selected_lod() const override;
    std::uint16_t part_flags(unsigned index, unsigned lod) const override;
    VehicleAnchorPart part(unsigned index, unsigned lod) const override;
    float maximum_vertex_z(unsigned index, unsigned lod) const override;
};
} // namespace sgn::retail
