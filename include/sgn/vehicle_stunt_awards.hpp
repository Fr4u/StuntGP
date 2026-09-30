#pragma once
#include "sgn/vehicle_score.hpp"
#include "sgn/vehicle_stunt_pattern.hpp"

namespace sgn::retail
{
struct VehicleStuntRewards
{
    std::array<std::uint32_t, 33> points{};      // 0060D688
    std::array<std::string_view, 93> messages{}; // 006127A0
    float normal_energy{}, upgraded_energy{};    // 0060F2D8/E0
};
struct VehicleStuntChainAudio
{
    virtual ~VehicleStuntChainAudio() = default;
    // 00461150(sound,0,0,255,0,1,0): fixed arguments differ from score sound.
    virtual void play_chain(std::uint32_t sound_id) = 0;
};
std::int32_t vehicle_random_integer(VehicleRandom &, std::int32_t amplitude) noexcept; // 0042f1f0
std::uint32_t stunt_chain_points(std::uint32_t base, std::uint32_t chain);             // 00411da0
void reward_stunt_energy(VehicleState &, std::uint32_t percent);                       // 004190b0
void award_stunt_pattern(VehicleState &, contact::player::Controls &, std::uint32_t vehicle_id, VehicleScoreState &,
                         const VehicleStuntContext &, const VehicleStuntRewards &, VehicleRandom &,
                         VehicleStuntChainAudio &, const VehicleStuntPattern &, unsigned repetitions); // 00411de0
void recognize_stunt(VehicleState &, contact::player::Controls &, std::uint32_t vehicle_id, VehicleScoreState &,
                     const VehicleStuntContext &, const VehicleStuntRewards &, VehicleRandom &,
                     VehicleStuntChainAudio &); // 00411f10
void complete_stunt_landing(VehicleState &, contact::player::Controls &, std::uint32_t vehicle_id, VehicleScoreState &,
                            const VehicleStuntContext &, const VehicleStuntRewards &, VehicleRandom &,
                            VehicleStuntChainAudio &); // 00412040
void update_stunt_landing(VehicleState &, contact::player::Controls &, std::uint32_t vehicle_id, VehicleScoreState &,
                          const VehicleStuntContext &, const VehicleStuntRewards &, VehicleRandom &,
                          VehicleStuntChainAudio &); // 004120f0
} // namespace sgn::retail
