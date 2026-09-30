#pragma once
#include "sgn/contact_player.hpp"
#include "sgn/track_contact.hpp"
#include "sgn/vehicle.hpp"
#include "sgn/retail_race.hpp"

namespace sgn::retail::contact::surface
{
struct State
{
    std::uint32_t saved_controller_mode{};   // +7630
    std::optional<std::int32_t> object_mode; // pointed-to value at +726C, when present
    std::uint32_t special_ticks{}, saved_2278{};
    std::array<std::uint32_t, 3> special_accumulator{};
    std::uint32_t saved_24a4{};
};
// Physical/controller prefix of 00408E90. True requires immediate 00411340.
// That scoring/vehicle procedure is a separate dependency, never an empty hook.
bool attach_controller(player::Controller &, State &, VehicleState &);
void detach_controller(player::Controller &, State &, VehicleState &);             // full 00408F90
void check_direction(VehicleRaceState &, Vec3 vehicle_forward, std::int32_t mode); // full 00418510
// Required synchronous dependency. The implementation must execute the real
// 00411340 vehicle/scoring procedure; a notification queue is not a replacement.
class InvertedContact
{
  public:
    virtual ~InvertedContact() = default;
    virtual void apply(VehicleState &, player::Controls &, player::VehicleId) = 0;
};
void process_material(VehicleState &, VehicleRaceState &, State &, player::Controls &, player::VehicleId, Material,
                      float alignment, std::int32_t mode, InvertedContact &); // 00408FC0
} // namespace sgn::retail::contact::surface
