#pragma once
#include "sgn/vehicle_math.hpp"
#include <optional>

namespace sgn::retail
{
// Per-level source pointer identities represented relative to the single
// ModelResource normal owner. The base62448C is the borrowed span's beginning;
// first_generated is624490.426ED0 publishes cursor576CC8 before animation.
struct TrackNormalAllocation
{
    std::uint32_t first_generated{};
    std::optional<std::uint32_t> cursor;
};
struct TrackNormalView
{
    std::span<VehicleVector> normals;
    TrackNormalAllocation &allocation;
};
// Full42EF80 scalar binding from the actual track PMD. It does not initialize
// cursor576CC8, generate normals or allocate/copy a second vector table.
void bind_track_normal_allocation(std::span<const std::uint8_t> pmd, std::span<VehicleVector> model_normals,
                                  TrackNormalAllocation &);
} // namespace sgn::retail
