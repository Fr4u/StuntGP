#pragma once
#include "sgn/vehicle_reset.hpp"

namespace sgn::retail
{
struct RaceSessionState;
class TrackSplineSource;
// NEW host adapter (R18 WP-D, DESIGN A6): the immutable-per-call
// VehicleResetParameters of the native 418830/418970 tick on the single menu
// world. Field-for-field RaceSession::reset_parameters() (race_session.cpp:314-332)
// except that the spline is resolved from the live source descriptor 0 of
// track.splines ([624474], TrackSplineSource::first()) and the spacing word is
// the live allocation DWORD [624474]+24 (TrackSplineSource::reset_spacing_word()).
// The contained SplineView is CALL-SCOPED (it borrows the live allocation): build
// it per 418A90 call and never store it in persistent state.
VehicleResetParameters source_vehicle_reset_parameters(const RaceSessionState &, const TrackSplineSource &);
} // namespace sgn::retail
