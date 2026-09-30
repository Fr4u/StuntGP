#pragma once
#include "sgn/contact_rail_particles.hpp"
#include "sgn/contact_rail_projection.hpp"
#include "sgn/retail_audio_driver.hpp"
#include "sgn/vehicle_reset.hpp"
#include "sgn/track_spline_source.hpp"

namespace sgn::retail
{
struct RailParameters
{
    float target_speed{}, refuel_limit{}; // 60F2E4/E8, borrowed level configuration
    Vec3 gravity{};                       // 482678; unit gravity already belongs to reset.parameters
    std::string_view begin_text;          // localized 6127F4
};
struct RailWorld
{
    VehicleResetWorld &reset;
    std::span<const VehicleParameters, 6> parameters;
    std::span<VehicleRailState, 6> vehicles;
    RailState &state;
    const TrackSplineSource &splines; // R18: live track owner; tube labels belong to its 418B10
    const RailParameters &configuration;
    contact::effects::ImpactParticles &particles;
    VehicleRandom &random;
    AudioDriver &audio;
    CameraMatrix &shared_model_identity; // 419310 ->403310 ->44F4D9
    contact::solver::PayloadAudit *payload_audit{};
};
struct RailEmission
{
    std::optional<unsigned> primary, anchor;
};
// Full 418F70 and its existing particle callees. Source's local velocity W is
// unwritten: the anchor destination retains its opaque word and is marked below.
RailEmission emit_rail_particles(contact::effects::ImpactParticles &, VehicleRandom &, const VehicleState &,
                                 const VehicleRailState &);
// Decoded-file form of 4192F0/418B10 (DecodedTrackSplines::initialize_rail and
// tests). The tick reaches 4192F0 through RailWorld::splines.initialize_rail.
void initialize_rail(RailState &, const SplineSet &, std::span<const std::string> raw_texture_labels); // 4192F0/418B10
void begin_rail_notice(RailWorld &);                                                                   // 419110
void update_rail_audio(RailWorld &);  // 418BD0, full positioned audio driver
void update_rail_refuel(RailWorld &); // 4191F0
struct RailUpdateAudit
{
    std::optional<unsigned> lens_controller;
};
void update_rail(RailWorld &, RailUpdateAudit *audit = nullptr); // 419310
} // namespace sgn::retail
