#pragma once
#include "sgn/retail_vehicle_load.hpp"
#include "sgn/retail_level_selection.hpp"

namespace sgn::retail
{
// Sole world owner of the former template pointer474630 and config ID472C74.
// Values must come from the enclosing startup; value initialization alone is
// not a claim about the original cold-world global initialization.
struct VehicleLoadSelection
{
    unsigned template_vehicle{};
    std::int32_t configuration_id{};
};
// Full40BEC0 calls full40BDE0 for active==1 in physical order. Restores only
// template_vehicle; configuration_id and world.current retain the last load.
void reload_selected_vehicle_configurations(VehicleLoadWorld &, const AssetStore &, std::span<const LevelCarSlot, 6>,
                                            VehicleLoadSelection &, std::int32_t control_kind);
} // namespace sgn::retail
