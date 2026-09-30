#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace sgn::retail
{
// Single level owners. Simulation, camera and presentation borrow these tables;
// their consumers must not retain separately mutable copies of the same words.
struct LevelCurveTables
{
    std::array<float, 128> fog{};                       // 5896C8
    std::array<float, 128> restitution{};               // 497D20
    std::array<float, 256> far_clip{};                  // 577688
    std::array<float, 128> respawn{};                   // 60DFF0
    std::array<float, 128> lights{};                    // 60E1F0
    std::array<std::uint32_t, 128> ambience{};          // 60E3F0: float bits copied by415770
    std::uint32_t ambience_adjacent_word{};             // 60E5F0, preserved: not written by this loader
    std::array<float, 256> width_left{}, width_right{}; // 60E7F0/60EBF0
};
// Eight actual446926..4469D1 array loads through434A70. Finite, bounded authored
// configuration domain. Missing/malformed text throws instead of reproducing
// the original invalid-pointer walk. No fallback values or synthetic padding.
// Ambience's adjacent source word remains the caller's existing value.
void apply_level_curves(std::string_view, LevelCurveTables &);
} // namespace sgn::retail
