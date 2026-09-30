#pragma once
#include "sgn/vehicle.hpp"
#include "sgn/contact_player.hpp"
#include <array>
#include <string_view>

namespace sgn::retail
{
// Recovered 00401220 presentation state. Names without a known interpretation
// retain their offset; untouched fields remain intact across slot reuse.
// Vehicle and texture identities are stable IDs, never guest addresses.
struct VehicleScorePopup
{
    std::uint32_t active{}, vehicle{}, variant{}, category{}, flags{};
    std::array<float, 3> offset{};
    std::uint32_t reserved_20{};
    float growth{};
    std::uint32_t age{}, lifetime{}, reserved_30{}, points{};
    std::array<char, 8> text{};
    std::uint32_t enabled{};
    float scale{}, angle{}, depth{};
    std::uint32_t reserved_50{};
    std::array<float, 3> color_animation{};
    std::uint32_t reserved_60{};
    std::array<float, 3> motion{};
    std::uint32_t motion_flags{};
    std::array<float, 2> text_size{};
    std::array<std::uint32_t, 3> colors{};
    std::uint32_t font{}, alignment{}, style{}, reserved_94{}, overlay_enabled{}, overlay_age{};
    std::array<float, 3> overlay_size{};
    std::uint32_t presentation_flags{};
    std::array<float, 2> position{};
    std::uint32_t reserved_b8{};
    std::array<float, 2> dimensions{};
    std::array<float, 4> texture_rect{};
    std::uint32_t texture{}, presentation_lifetime{}, presentation_x{}, presentation_y{};
    std::array<float, 2> presentation_size{}, presentation_scale{};
    std::array<std::uint32_t, 17> reserved_f4{};
};
struct VehicleScoreState
{
    std::array<VehicleScorePopup, 16> popups{};
    std::uint32_t sound_class{}, sound_id{}, sound_volume{}; // 00497D10/14/18
};
// Device playback is a new platform boundary for 00461150. The source cache
// above is updated synchronously before this request; no retail RNG is removed.
struct VehicleScoreAudio
{
    virtual ~VehicleScoreAudio() = default;
    virtual void play(std::uint32_t sound_id, std::uint32_t volume) = 0;
};
struct VehicleStuntContext
{
    std::int32_t game_mode{}, race_mode{}, score_block{}, hud_block{};
    std::uint32_t tick{}, inversion_points{};
    std::string_view inversion_text;
    VehicleScoreAudio &audio;
};
void clear_score_popups(VehicleScoreState &) noexcept; // 00401000, retains sound cache
void award_stunt_score(VehicleState &, const contact::player::Controls &, std::uint32_t vehicle_id, VehicleScoreState &,
                       const VehicleStuntContext &, std::uint32_t category, std::uint32_t variant,
                       std::uint32_t points); // 00401220
void update_inversion_state(VehicleState &, contact::player::Controls &, std::uint32_t vehicle_id, VehicleScoreState &,
                            const VehicleStuntContext &, Vec3 unit_gravity); // 00411340
void award_stunt_height(VehicleState &, const contact::player::Controls &, std::uint32_t vehicle_id,
                        VehicleScoreState &, const VehicleStuntContext &,
                        std::span<const std::uint32_t, 33> point_values, float height); // 004115e0
void update_stunt_height(VehicleState &, const contact::player::Controls &, std::uint32_t vehicle_id,
                         VehicleScoreState &, const VehicleStuntContext &,
                         std::span<const std::uint32_t, 33> point_values); // 00411680
} // namespace sgn::retail
