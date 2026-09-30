#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace sgn::retail::gameplay_tables
{
struct Rule
{
    std::string_view name;
    std::uint32_t scale_bits, index;
};
inline constexpr std::array<Rule, 30> gameplay{{
    {"human_replace_energy", 0x41200000u, 1u},
    {"human_stunt_energy_award", 0x3f800000u, 2u},
    {"cpu_replace_energy", 0x41200000u, 3u},
    {"cpu_stunt_energy_award", 0x3f800000u, 4u},
    {"car_pitlane_speed", 0x3eed097cu, 5u},
    {"car_pitlane_recharge_total", 0x41200000u, 6u},
    {"car_victorylap_speed", 0x3eed097cu, 7u},
    {"car_rollingstart_speed", 0x3eed097cu, 8u},
    {"cpu_skill_collision_nrg_scale", 0x3c23d70au, 9u},
    {"cpu_skill_engine_nrg_scale", 0x3c23d70au, 10u},
    {"cpu_skill_boost_nrg_scale", 0x3c23d70au, 11u},
    {"cpu_skill_boostusage_scale", 0x3c23d70au, 12u},
    {"cpu_skill_engine_torque_scale", 0x3c23d70au, 13u},
    {"cpu_skill_aerodrag_scale", 0x3c23d70au, 14u},
    {"cpu_skill_torquemap3_select", 0x3f800000u, 15u},
    {"cpu_skill_torquemap2_select", 0x3f800000u, 16u},
    {"cpu_skill_torquemap1_select", 0x3f800000u, 17u},
    {"cpu_catchup_distance_behind_max", 0x42c80000u, 18u},
    {"cpu_catchup_distance_behind_skill_maxsub", 0x3c23d70au, 19u},
    {"cpu_catchup_distance_ahead_max", 0x42c80000u, 20u},
    {"cpu_catchup_distance_ahead_skill_maxsub", 0x3c23d70au, 21u},
    {"cpu_catchup_setup_scale", 0x3c23d70au, 22u},
    {"human_catchup_distance_behind_max", 0x42c80000u, 23u},
    {"human_catchup_setup_scale", 0x3c23d70au, 24u},
    {"collision_energy_min", 0x41200000u, 25u},
    {"collision_energy_max", 0x41200000u, 26u},
    {"collision_energy_scale", 0x3f800000u, 27u},
    {"collision_energy_impact_limit", 0x41200000u, 28u},
    {"collision_energy_impact_timedelay", 0x42700000u, 29u},
    {"cpu_autobraking_disable", 0x3f800000u, 30u},
}};
// clang-format off: one Rule record per row, in index order
inline constexpr std::array<Rule,36> scores{{
    {"score_backflip_360",0x00000000u,1u},
    {"score_backflip_720",0x00000000u,2u},
    {"score_backflip_1080",0x00000000u,3u},
    {"score_frontflip_360",0x00000000u,4u},
    {"score_frontflip_720",0x00000000u,5u},
    {"score_frontflip_1080",0x00000000u,6u},
    {"score_twistright_360",0x00000000u,7u},
    {"score_twistright_720",0x00000000u,8u},
    {"score_twistright_1080",0x00000000u,9u},
    {"score_twistleft_360",0x00000000u,10u},
    {"score_twistleft_720",0x00000000u,11u},
    {"score_twistleft_1080",0x00000000u,12u},
    {"score_noseup",0x00000000u,13u},
    {"score_nosedown",0x00000000u,14u},
    {"score_halftwist_left",0x00000000u,15u},
    {"score_halftwist_right",0x00000000u,16u},
    {"score_twist_combo",0x00000000u,17u},
    {"score_flip_combo",0x00000000u,18u},
    {"score_freestyle",0x00000000u,19u},
    {"score_freestyle_combo",0x00000000u,20u},
    {"score_slide",0x00000000u,21u},
    {"score_powerslide",0x00000000u,22u},
    {"score_stream",0x00000000u,23u},
    {"score_superstream",0x00000000u,24u},
    {"score_trickstart",0x00000000u,25u},
    {"score_barrelroll",0x00000000u,26u},
    {"score_grind",0x00000000u,27u},
    {"score_flightheight_1",0x00000000u,28u},
    {"score_flightheight_2",0x00000000u,29u},
    {"score_flightheight_3",0x00000000u,30u},
    {"score_flightheight_4",0x00000000u,31u},
    {"score_loopdeloop",0x00000000u,32u},
    {"score_twowheel_left",0x00000000u,33u},
    {"score_twowheel_right",0x00000000u,34u},
    {"score_twowheel_front",0x00000000u,35u},
    {"score_twowheel_rear",0x00000000u,36u},
}};
// clang-format on
} // namespace sgn::retail::gameplay_tables
