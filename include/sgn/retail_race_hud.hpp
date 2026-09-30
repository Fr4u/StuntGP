#pragma once
#include "sgn/retail_time_trial.hpp"
namespace sgn::retail
{
struct HudElementLayout
{
    float x{}, y{};
    std::uint32_t word_14{}, word_18{}, word_1c{}, word_20{};
};
// Eighteen36-byte source elements begin at HUD+10. Countdown element0 state
// stays in ControllerHudCountdown. Element6 active/phase stay in the existing
// Controller.respawn_transition. The arrays below omit those exact fields.
struct RaceHudLayout
{
    std::array<HudElementLayout, 18> elements{};
    std::array<std::uint32_t, 16> active{}, phase{};
    std::array<float, 17> value{};
};
struct RaceHudMargins
{
    std::int32_t x{}, y{};
}; // 6244FC/624500, original signed FILD
struct RaceHudView
{
    contact::player::Controls &controls;
    std::span<ControllerHudCountdown, 4> countdown;
    std::span<RaceHudLayout, 4> layouts;
    unsigned &selected_hud; // 61B740 identity; distinct from Controls.current
    RaceHudMargins &margins;
};
struct HudElementView
{
    HudElementLayout &layout;
    std::uint32_t &active;
    float &value;
    std::uint32_t &phase;
};
HudElementView hud_element(RaceHudView, unsigned hud, unsigned element);
void initialize_selected_hud_layout(RaceHudView);  // full4478C0, retains current-controller asymmetry
void initialize_all_hud_layouts(RaceHudView);      // full447A60
void initialize_selected_hud_ranking(RaceHudView); // full449E80
void initialize_race_hud(RaceHudView);             // full448110, including actual notice resets
void enable_selected_race_hud(RaceHudView);        // 447B50
void arm_race_hud_countdown(RaceHudView, std::span<const VehicleState, 6>, std::uint32_t ticks); // full447BD0
} // namespace sgn::retail
