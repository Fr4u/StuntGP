#include "sgn/retail_vehicle_bootstrap.hpp"
#include "sgn/model_anchor_adapter.hpp"
#include <utility>

namespace sgn::retail
{
void initialize_vehicle_model_anchors(VehicleStorage &storage, ModelResources &models, unsigned &current)
{
    std::array<std::optional<ModelAnchorAdapter>, 6> adapters;
    for (unsigned i = 0; i < 6; ++i)
        if (storage.race[i].active == 1)
        {
            if (!storage.models[i].model)
                throw std::domain_error("active bootstrap vehicle requires a model binding");
            const auto handle = *storage.models[i].model;
            if (models.source_backed(handle))
                adapters[i].emplace(models.source_at(handle));
            else
                adapters[i].emplace(models.at(handle));
        }
    auto views = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<VehicleAnchorVehicle, 6>{{{storage.race[I], storage.presentation[I], storage.trail_inputs[I],
                                                     storage.anchors[I], adapters[I] ? &*adapters[I] : nullptr}...}};
    }(std::make_index_sequence<6>{});
    VehicleAnchorWorld anchors{views, current};
    initialize_vehicle_extent_anchors(anchors); // 413785 ->417E10
    initialize_vehicle_effect_anchors(anchors); // 41378A ->4162C0
    initialize_vehicle_pair_a_anchors(anchors); // 41378F ->414F00
    initialize_vehicle_pair_b_anchors(anchors); // 413794 ->4150D0
}
} // namespace sgn::retail
