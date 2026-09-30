#pragma once
#include "sgn/retail_race_start.hpp"
#include "sgn/vehicle_prestart_progression.hpp"
#include "sgn/vehicle_race_startup_state.hpp"
#include "sgn/retail_race_hud.hpp"
#include <stdexcept>

namespace sgn::retail
{
// Sole4856C0/4856D8/4856F0/485708 source pointer banks, including PE initial pointers.
// The old preview row API projects these words; it owns no parallel indices.
struct RaceResultTextPointers
{
    std::array<std::uint32_t, 24> words{{0x4857b8u, 0x4857c4u, 0x4857d0u, 0x4857dcu, 0x4857e8u, 0x4857f4u,
                                         0x485800u, 0x485808u, 0x485810u, 0x485818u, 0x485820u, 0x485828u,
                                         0x485830u, 0x485838u, 0x485840u, 0x485848u, 0x485850u, 0x485858u,
                                         0x485860u, 0x485868u, 0x485870u, 0x485878u, 0x485880u, 0x485888u}};
    struct Row
    {
        RaceResultTextPointers &bank;
        unsigned row;
        bool has_value() const noexcept
        {
            const auto d = bank.words[row] - 0x60f050u;
            return d < 600u && d % 100u == 0;
        }
        explicit operator bool() const noexcept
        {
            return has_value();
        }
        unsigned operator*() const
        {
            if (!has_value())
                throw std::out_of_range("result name pointer is not a car-name identity");
            return (bank.words[row] - 0x60f050u) / 100u;
        }
        Row &operator=(unsigned slot)
        {
            if (slot >= 6)
                throw std::out_of_range("result vehicle slot");
            for (unsigned c = 0; c < 4; ++c)
                bank.words[c * 6 + row] = 0x60f050u + slot * 100u + c * 16u;
            return *this;
        }
        Row &operator=(std::nullopt_t)
        {
            for (unsigned c = 0; c < 4; ++c)
                bank.words[c * 6 + row] = 0;
            return *this;
        }
    };
    struct ConstRow
    {
        const RaceResultTextPointers &bank;
        unsigned row;
        bool has_value() const noexcept
        {
            const auto d = bank.words[row] - 0x60f050u;
            return d < 600u && d % 100u == 0;
        }
        explicit operator bool() const noexcept
        {
            return has_value();
        }
        unsigned operator*() const
        {
            if (!has_value())
                throw std::out_of_range("result name pointer is not a car-name identity");
            return (bank.words[row] - 0x60f050u) / 100u;
        }
    };
    Row operator[](unsigned row)
    {
        return {*this, row};
    }
    ConstRow operator[](unsigned row) const
    {
        return {*this, row};
    }
};
struct NormalRaceFinishState
{
    std::uint32_t row_ticks{}, visible_rows{}, field_60d728{}; // 60D720/724/728
    std::uint32_t confirm_consumed{}, confirm_pressed{};       // 612964/968
    std::uint32_t field_612bf0{};
    std::uint32_t menu_requested{}; // sole612BF4; frontend stops at pending scene transition
    std::uint32_t field_612cf8{};
    RaceResultTextPointers result_rows; // four canonical pointer banks; no index mirror
};
struct NormalRacePresentation
{
    virtual ~NormalRacePresentation() = default;
    virtual void music(std::uint32_t track, std::uint32_t loop) = 0; // 45F8F0
    virtual void fade_music() = 0;                                   // 45F8E0(0,-10)
    virtual void finish_camera() = 0;                                // 443DD0
    virtual void frequency_sound(std::uint32_t id, std::uint32_t frequency, std::uint32_t volume) = 0;
};
struct NormalRaceLifecycleWorld
{
    RaceStartWorld &race;
    std::span<VehicleRaceStartupState, 6> startup;
    std::uint32_t &phase_ticks; // sole60F2C4
    std::uint32_t &selected_hud;
    std::uint32_t game_type{}, camera_mode{}, hud_block{};
    RaceHudView hud;
    std::span<ControllerOverlayTransition, 4> overlays;
    std::span<LevelCarSlot, 6> cars;
    const VehicleRaceFinishState &finish;
    NormalRaceFinishState &presentation_state;
    std::uint32_t &retire_exit;  // 60D71C
    std::uint32_t &finish_music; // 60F2C0
    LevelAudioState &audio;
    const std::uint32_t &music_active; // 62F5AC, updated by music calls
    std::uint32_t split{};             // 576BE0
    NormalRacePresentation *services{};
};
void start_normal_race(NormalRaceLifecycleWorld &); // 4439F0 type2 ->443950
// Local single-player normal-race443A80, including finish, retirement, podium
// and results. Platform music/camera services are required on entered branches.
void tick_normal_race_lifecycle(NormalRaceLifecycleWorld &);
} // namespace sgn::retail
