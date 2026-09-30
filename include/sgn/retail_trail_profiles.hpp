#pragma once
#include "sgn/vehicle_trails.hpp"

namespace sgn::retail
{
struct TrailRenderProfile
{
    std::uint32_t mode{}, capacity{}, stride{}, max_segments{}, shape_identity{};
    std::uint32_t flags{}, texture{};
    std::array<float, 4> uv{};             // u0,v0,u1,v1
    std::array<float, 16> width{};         // 2C..68; next segment uses j+1
    std::array<float, 16> alpha{};         // 6C..A8
    std::array<std::uint32_t, 16> color{}; // AC..E8
    std::uint32_t head_enabled{}, head_flags{}, head_texture{}, head_alpha{}, head_color{};
    std::array<float, 3> head_scale{};
    std::uint32_t opaque_10c{};
};
static_assert(sizeof(TrailRenderProfile) == 272 && offsetof(TrailRenderProfile, width) == 0x2c);
static_assert(offsetof(TrailRenderProfile, head_enabled) == 0xec);
struct TrailShape
{
    std::uint32_t identity{};
    std::array<VehicleVector, 4> points;
};
struct TrailProfileBank
{
    std::array<TrailRenderProfile, 9> profiles; // sole474AF8..475488 (exclusive)
    std::array<TrailShape, 3> shapes;           // 474988/474A78/4749C8; identities are never host pointers
};
TrailProfileBank initial_trail_profile_bank();
VehicleTrailSettings vehicle_trail_settings(const TrailProfileBank &) noexcept;
std::array<VehicleTrailParameters, 9> impact_trail_parameters(const TrailProfileBank &) noexcept;
const std::array<VehicleVector, 4> &trail_shape(const TrailProfileBank &, std::uint32_t identity);
} // namespace sgn::retail
