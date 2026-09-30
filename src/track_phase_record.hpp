#pragma once
#include "sgn/track_part_animation.hpp"
namespace sgn::retail::detail {
//Shared existing427140 record math. Caller owns admission and kind observation.
void advance_track_phase_record(TrackPartAnimation&,std::uint32_t captured_kind);
}
