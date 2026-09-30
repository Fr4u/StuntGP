#pragma once
#include "sgn/race_session.hpp"
#include "sgn/vehicle_tick_composition.hpp"
#include "sgn/track_spline_source.hpp"

namespace sgn::retail
{
// One-call resource view. No copied persistent world, source pointers, clock
// advance or host input policy. The real session and the differential fixture
// invoke this same adapter with their own already initialized source owners.
// R18 (DESIGN A1/A7/A8): splines is the track owner (SourceTrackSplines on the
// single world, DecodedTrackSplines for tests/preview); tube labels belong to
// its 4192F0/418B10. strip_stage, when set, replaces native 418945/4189BB.
struct RaceSessionVehicleResources
{
    ModelResources &models;
    const TrackSplineSource &splines;
    const contact::TrackGeometry &geometry;
    AudioDevice &audio_device;
    const VehicleResetParameters &reset_parameters;
    const VehicleTrailProjection &trail_projection;
    AudioRequestObserver *audio_observer{};
    VehicleStripStage *strip_stage{};
    VehicleParticleStage *particle_stage{}; // R21: replaces native 418922..41893B/41899D..4189B6
};
VehicleTickObservation tick_session_vehicle(RaceSessionState &, const RaceSessionVehicleResources &,
                                            VehicleTickAudit *audit = nullptr);
} // namespace sgn::retail
