#pragma once
#include "sgn/retail_race.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail::race_oracle
{
template <class Record> void transfer(Record &b, VehicleRaceState &s, bool read)
{
    const auto u = [&](std::uint32_t &value, std::size_t at) {
        if (read)
            std::memcpy(&value, b.data() + at, 4);
        else
            std::memcpy(b.data() + at, &value, 4);
    };
    const auto i = [&](std::int32_t &value, std::size_t at) {
        auto raw = std::bit_cast<std::uint32_t>(value);
        u(raw, at);
        if (read)
            value = std::bit_cast<std::int32_t>(raw);
    };
    const auto f = [&](float &value, std::size_t at) {
        auto raw = std::bit_cast<std::uint32_t>(value);
        u(raw, at);
        if (read)
            value = std::bit_cast<float>(raw);
    };
    i(s.active, 0x1ba8);
    i(s.control_enabled, 0x2278);
    i(s.control_kind, 0x227c);
    u(s.saved_camera_mode, 0x22c8);
    i(s.race_index, 0x23a4);
    i(s.previous_progress, 0x23a8);
    i(s.current_progress, 0x23ac);
    f(s.progress, 0x23b0);
    f(s.previous_fraction, 0x23b8);
    i(s.lap_index, 0x23bc);
    i(s.lap_timer, 0x23c4);
    i(s.split_timer, 0x23c8);
    i(s.race_timer, 0x23cc);
    u(s.stunt_score, 0x23d0);
    u(s.best_lap_ticks, 0x23d4);
    u(s.total_ticks, 0x23d8);
    for (unsigned n = 0; n < 24; ++n)
        i(s.lap_times[n], 0x23dc + n * 4);
    u(s.spline_index, 0x24a4);
    u(s.progress_crossing, 0x24a8);
    const auto v = [&](VehicleVector &value, std::size_t at) {
        f(value.x, at);
        f(value.y, at + 4);
        f(value.z, at + 8);
        f(value.w, at + 12);
    };
    v(s.respawn_position, 0x24b0);
    v(s.respawn_heading, 0x24c0);
    v(s.route_right, 0x24d0);
    for (unsigned n = 0; n < 3; ++n)
    {
        i(s.route_indices[n], 0x2644 + n * 8);
        f(s.route_fractions[n], 0x2648 + n * 8);
    }
    u(s.finish_state, 0x24e4);
    u(s.stopped, 0x24e8);
    u(s.timing26a8, 0x26a8);
    u(s.event7678, 0x7678);
    u(s.automatic_drive, 0x767c);
    u(s.start_countdown, 0x7688);
    u(s.notice_active, 0x76f0);
    u(s.notice_ticks, 0x76f4);
    u(s.respawn_requested, 0x763c);
    u(s.collision_hold, 0x7640);
    u(s.input_hold, 0x7644);
    u(s.pending_reset, 0x7680);
    u(s.opposing_direction_ticks, 0x7634);
    u(s.unsafe_contact_ticks, 0x7638);
}
} // namespace sgn::retail::race_oracle
