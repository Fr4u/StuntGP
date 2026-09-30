#pragma once
#include <array>
#include <cstdint>
namespace sgn::retail::contact::effects
{
// Single per-car owner. Source arithmetic wraps; branches may interpret signed.
struct VehicleParticleState
{
    std::uint32_t field_761c{}, field_7620{}, field_7628{};
    std::array<float, 4> field_1ff8{}; // wheel1FF8+i98, updated by415880
};
} // namespace sgn::retail::contact::effects
