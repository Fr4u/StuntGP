#pragma once
#include <cstdint>
namespace sgn::retail
{
class FrontendState;
std::uint32_t source_car_available_437230(FrontendState &, std::uint32_t car, std::uint32_t mode);
std::uint32_t source_track_available_4371c0(FrontendState &, std::uint32_t track, std::uint32_t mode);
std::uint32_t source_track_value_437da0(std::uint32_t track) noexcept;
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t source_track_type_437360(std::uint32_t track) noexcept;
}
