#include "sgn/retail_vehicle_load_selection.hpp"
#include <bit>

namespace sgn::retail
{
void reload_selected_vehicle_configurations(VehicleLoadWorld &world, const AssetStore &assets,
                                            std::span<const LevelCarSlot, 6> cars, VehicleLoadSelection &selection,
                                            std::int32_t control_kind)
{
    const auto saved_template = selection.template_vehicle;
    for (unsigned i = 0; i < 6; ++i)
    {
        if (world.vehicles[i].race.active != 1)
            continue;
        selection.template_vehicle = i;
        selection.configuration_id = std::bit_cast<std::int32_t>(cars[i].settings.config);
        load_vehicle_configuration(world, assets, i, selection.configuration_id, control_kind);
    }
    selection.template_vehicle = saved_template;
}
} // namespace sgn::retail
