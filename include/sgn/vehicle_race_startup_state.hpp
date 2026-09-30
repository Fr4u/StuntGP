#pragma once
#include <array>
#include <cstdint>

namespace sgn::retail
{
// Original 40D410 writes 2280 after the conditional control enable. Its meaning
// beyond that write is not yet reconstructed. 2284..2297 have no inferred owner.
// 443D90 clears the 25 words below. The neighbouring 7688,76F0,76F4 words remain
// exclusively owned by VehicleRaceState.
struct VehicleRaceStartupState
{
    std::uint32_t field_2280{};
    std::array<std::uint32_t, 25> fields_768c{};
};
} // namespace sgn::retail
