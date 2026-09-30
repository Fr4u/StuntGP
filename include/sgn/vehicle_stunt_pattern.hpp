#pragma once
#include "sgn/vehicle.hpp"
#include <array>

namespace sgn::retail
{
struct VehicleStuntPattern
{
    std::array<std::uint32_t, 3> messages{}, score_indices{};
    std::array<std::int8_t, 64> pitch{}, yaw{};
};
unsigned match_stunt_pattern(const VehicleState &, const VehicleStuntPattern &); // 00411c10
} // namespace sgn::retail
