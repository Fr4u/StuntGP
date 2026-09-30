#pragma once
#include "sgn/source_track_publications.hpp"
#include "sgn/retail_track_geometry_animation.hpp"

namespace sgn::retail
{
// Synchronous borrows, not new payload owners. The caller supplies the SAME
// session's optional576CD0, persistent576CC8 and existing effect/RNG owners.
// An absent record bank rejects only the reached first record read; no warm
// initialization, zero records, pose or normal table is manufactured here.
struct RaceSessionState;
// Keep61297C live until426A00 actually reaches its rate read. The native
// preview value type is deliberately not reused for this source borrow.
struct SourceTrackAnimationEffects
{
    VehicleRandom &random;
    contact::effects::EffectParticleBank &particles;
    const WorldGravity &gravity;
    const std::uint32_t &source_rate;
    const ModelTransform &template_pose;
};
struct SourceTrackAnimationOwners
{
    std::optional<TrackPartAnimationBank> &records;
    SourcePmdLocation &generated_cursor; // sole576CC8, actual weak bank location
    SourceTrackAnimationEffects effects;
    TrackGeometryAnimationAudit audit{};
};
// Concrete binding to the SAME persistent platform world; no initialization.
SourceTrackAnimationOwners borrow_source_track_animation_owners(RaceSessionState &, TrackGeometryAnimationAudit = {});
// Full426ED0 and its reachable children426370..426E30. Source pointers are
// captured/reloaded at their instructions; all geometry stores reach the
// existing PMD bank. The input descriptor, last-loader globals and part+48
// are independent. No ModelResource conversion or geometry cache is used.
// Uses the existing PC53/nearest numerical helpers and undefined-stack-W
// policy (retain destination W). Hardware-FP-trap/intermediate-fault parity
// and foreign/wrapping source addresses are outside this host domain.
void animate_source_track_geometry_426ed0(SourceAllocationBank &, const SourcePmdLoadingState &,
                                          SourceTrackModelPointer incoming_descriptor, SourceTrackAnimationOwners &);
} // namespace sgn::retail
