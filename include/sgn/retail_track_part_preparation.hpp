#pragma once
#include "sgn/vehicle_math.hpp"

namespace sgn::retail
{
struct TrackPartPreparationScratch
{
    // 5775D0..57764F: four position vectors interleaved with four pairs of
    // front/back UV coordinates.425AE0 clears all32 words before each scan.
    std::array<VehicleVector, 8> values{};
};
struct TrackPartPreparationView
{
    std::span<VehicleVector> vertices;                   // part+30, countpart+28
    std::span<std::array<std::uint32_t, 4>> coordinates; // part+2C
    std::span<std::array<std::uint8_t, 8>> polygons;     // part+34, countpart+24
    std::span<std::uint16_t> corners;                    // whole descriptor+38 owner
    std::span<const VehicleVector> normals;              // whole62448C owner
    TrackPartPreparationScratch &scratch;
};
// Full425DF0 ->425B50 ->425AE0 for the authored finite animated-track domain:
// even positive polygon count, bounded normal/corner indices, kinds1/2/3.
// This writes the actual vertex/UV/polygon/corner owners; no render-only mesh.
void prepare_animated_track_part(TrackPartPreparationView, std::uint32_t kind);
} // namespace sgn::retail
