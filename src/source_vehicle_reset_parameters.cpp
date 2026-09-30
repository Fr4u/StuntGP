#include "sgn/source_vehicle_reset_parameters.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_game_text_table.hpp"
#include "sgn/spline_resolution.hpp"
#include "sgn/track_spline_source.hpp"
#include <algorithm>
#include <bit>

namespace sgn::retail
{
namespace
{
// Same terminator rule as race_session.cpp:22-26 bounded_text (six car names).
template <std::size_t N> std::string_view bounded_text(const std::array<char, N> &a)
{
    const auto end = std::find(a.begin(), a.end(), '\0');
    if (end == a.end())
        throw FormatError("session name lacks terminator");
    return {a.data(), static_cast<std::size_t>(end - a.begin())};
}
} // namespace
VehicleResetParameters source_vehicle_reset_parameters(const RaceSessionState &s, const TrackSplineSource &splines)
{
    // [624474] = descriptor 0 of the live track.splines allocation. The first
    // unconditional reader on the tick is 418320 (4183A5 MOV EDI,[624474]),
    // so a missing descriptor is reported at that site.
    const auto spline = resolve_spline(splines.first(), 0x4183a5);
    if (!spline)
        throw FormatError("first track spline cannot be resolved");
    VehicleResetParameters p;
    p.stage = std::bit_cast<std::int32_t>(s.clock.phase);
    p.mode = std::bit_cast<std::int32_t>(s.clock.game_mode);
    p.selector = std::bit_cast<std::int32_t>(s.level.game_type);
    p.normal_energy_cost = s.gameplay.values[1];
    p.upgraded_energy_cost = s.gameplay.values[3];
    p.course_length = s.clock.course_length;
    // 418D20 reads [624474]+24 only after saved_24a4!=0. Absence (the DWORD is not
    // inside the live allocation) stays absent until that source branch needs it.
    p.spacing_word = splines.reset_spacing_word();
    p.spline = *spline;
    p.mask = s.curves.respawn;
    p.unit_gravity = s.gravity.unit;
    p.tick = s.simulation_tick;
    for (unsigned i = 0; i < 6; ++i)
        p.vehicle_names[i] = bounded_text(s.cars[i].name);
    p.special_end_text = s.game_text.required(GameTextId::rejoins_the_race);
    p.retired_text = s.game_text.required(GameTextId::is_out_of_the_race);
    return p;
}
} // namespace sgn::retail
