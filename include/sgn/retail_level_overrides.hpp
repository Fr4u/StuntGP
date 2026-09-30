#pragma once
#include "sgn/retail_level.hpp"
#include <cstdint>

namespace sgn::retail
{
struct LevelAudioState
{
    std::uint32_t scale{}, input_a{}, input_b{};           // 489790,62E0A8,62E0F0
    std::uint32_t output_a{}, output_b{}, output_b_copy{}; // 62E0F8,489798,48979C
};
struct LevelOverrideOptions
{
    std::uint32_t selection_phase{};                                     // 612BEC, exact ==1
    std::uint32_t music{}, audio_a{}, audio_b{}, render_a{}, render_b{}; // 60D730..740
    std::uint32_t texture{8}, split{}, laps{}, challenge_ticks{};        // 60D74C,750,764,768
    std::uint32_t record{}, record_f004{}, record_f008{}, record_f00c{}; // 60D76C..778
    std::uint32_t game_type{}, race_mode{}, start_delay{}, transient{};  // 60D77C..788
    std::uint32_t demo_override{}, display_selector{};                   // 60DF70,60DF94
};
// Transient initialization plan. Consume its results when creating the world;
// do not retain a second mutable copy of RaceClockState's live globals.
struct LevelOverrideState
{
    LevelSettings level;
    LevelAudioState audio;
    std::uint32_t start_delay{}, split{}, render_a{}, render_b{}, display_selector{};
    std::uint32_t race_mode{}, demo_ticks{}, demo_active{};
    std::uint32_t record_effc{}, record_f004{}, record_f008{}, record_f00c{};
};
void update_level_audio(LevelAudioState &, LevelOverrideOptions &) noexcept;       // 00460BC0
void apply_demo_override(LevelOverrideState &) noexcept;                           // 00445310
void apply_challenge_override(LevelOverrideState &, std::uint32_t ticks) noexcept; // 00445120
struct LevelOverrideDependencies
{
    virtual ~LevelOverrideDependencies() = default;
    virtual void prepare_time_trial(LevelOverrideState &) = 0; // 00445360
    virtual void prepare_cars(LevelOverrideState &) = 0;       // 00444DB0
};
// Full caller's scalar writes and dispatch. Required callees have no defaults;
// their full startup behavior needs separate qualification and real bindings.
void apply_level_overrides(LevelOverrideOptions &, LevelOverrideState &, LevelOverrideDependencies &); // 004453A0
} // namespace sgn::retail
