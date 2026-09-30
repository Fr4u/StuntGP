#pragma once
#include "sgn/assets.hpp"
#include "sgn/retail_camera.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace sgn::retail::contact::player
{
using VehicleId = std::uint32_t; // Stable slot identity replaces a retail vehicle pointer.
struct Notice
{
    std::int32_t remaining{}, initial{};
    std::uint32_t tick{};
    std::array<char, 64> text{};
    VehicleId source_vehicle{};
    std::int32_t audience{};
};
struct Controller
{
    std::int32_t active{};
    CameraState camera{};
    VehicleId vehicle{};
    struct RespawnTransition
    {
        std::uint32_t state{}, phase{}; // controller +300/+308, written by 00447AA0
    } respawn_transition;
    std::array<Notice, 8> notices{};
};
struct Controls
{
    std::array<Controller, 4> controllers{};
    unsigned current{};
};
std::optional<unsigned> find_controller(const Controls &, VehicleId); // 00402590 / 00442C50
void restore_primary(Controls &) noexcept;                            // 00402580
void clear_notices(std::array<Notice, 8> &) noexcept;                 // 00442C10
void post_notice(Controls &, std::int32_t game_mode, std::uint32_t tick, std::int32_t duration, std::string_view text,
                 VehicleId source, std::int32_t audience, std::int32_t priority); // 00442CF0 + 00442C80
} // namespace sgn::retail::contact::player
