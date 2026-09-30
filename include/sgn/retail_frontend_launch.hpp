#pragma once
#include "sgn/retail_frontend_state.hpp"

namespace sgn::retail
{
struct FrontendInputBinding
{
    std::uint32_t vehicle{}, kind{}; // 62D598/59C, physical input stride1DC
};
// Source words not already owned by options, trial selection or the six cars.
// This joins the persistent world at integration; never a second options bank.
struct FrontendLaunchState
{
    std::uint32_t setting_60d744{}, track_index{}, prepared{}, catalog_count{}; // 60D744/748,60DF6C/90
    std::array<FrontendInputBinding, 4> input_bindings{};
};
struct FrontendLaunchWorld
{
    FrontendSourceStorage &frontend;
    LevelOverrideOptions &options;
    std::span<SelectionRecord, 6> selection;
    TimeTrialSelectionState &trial;
    std::span<std::uint32_t, 3> other_selected_vehicles;
    FrontendLaunchState &state;
};
void clear_frontend_race_selection(FrontendLaunchWorld &); // full43A720/45ECC0
void bind_frontend_race_input(FrontendLaunchState &, std::uint32_t controller, std::uint32_t physical_vehicle,
                              std::uint32_t kind); // full43A760/45ECC0,controller1..4
void publish_frontend_vehicle_configuration(SelectionRecord &, std::span<const std::byte, 300> model,
                                            std::uint32_t identity, std::uint32_t livery); // full43A4A0
// Full43AF20 from the actual sole61BBE0/61BED0 owners. Stock AI configuration
// reaches full43A430/436490; the human consumes the roster's assembled model.
void select_campaign_race(FrontendLaunchWorld &);
} // namespace sgn::retail
