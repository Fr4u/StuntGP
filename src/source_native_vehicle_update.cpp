#include "sgn/source_native_vehicle_update.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session_vehicle_world.hpp"
#include "sgn/track_spline_source.hpp"
#include "sgn/source_vehicle_reset_parameters.hpp"
#include "sgn/source_vehicle_strip_producer.hpp"
#include "sgn/source_vehicle_particles.hpp"
#include "sgn/source_core_race_draw_remaining.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
namespace
{
// Same view the native 418830 step rebuilds per vehicle (race_session_vehicle_world.cpp).
VehicleTrailProjection projection(const RaceSessionState &s)
{
    if (s.controls.current >= s.camera_lifecycle.size())
        throw FormatError("vehicle update current controller outside camera owners");
    return {s.frame_world_view,    s.frame_projection[0],
            s.frame_projection[5], s.viewport.center_x,
            s.viewport.center_y,   s.viewport.scale_x,
            s.viewport.scale_y,    s.camera_lifecycle[s.controls.current].clip_near};
}
// DESIGN A7: skid strips of the single world are source-allocation bound and
// drawn by 41D1C0, so 418945/4189BB keep the exact 41D140 producer.
struct SourceStripStage final : VehicleStripStage
{
    RaceSessionPlatform &platform;
    explicit SourceStripStage(RaceSessionPlatform &p) : platform(p) {}
    void update(unsigned current) override
    {
        if (platform.state().current_slot != current)
            throw FormatError("strip stage current vehicle differs from traversal");
        update_source_vehicle_strip_41d140(platform);
    }
};
// R21: the particle children of the single world are the R17 source-owner
// translations (x87 compare polarity, unordered operands and IEEE division as
// the original), not the contact::effects adapters of the fixture worlds.
struct SourceParticleStage final : VehicleParticleStage
{
    RaceSessionPlatform &platform;
    explicit SourceParticleStage(RaceSessionPlatform &p) : platform(p) {}
    void emit(VehicleParticleChild child, unsigned current) override
    {
        if (platform.state().current_slot != current)
            throw FormatError("particle stage current vehicle differs from traversal");
        switch (child)
        {
        case VehicleParticleChild::engine:
            emit_source_engine_particles_416a80(platform);
            return;
        case VehicleParticleChild::tire:
            emit_source_tire_particles_415880(platform);
            return;
        case VehicleParticleChild::load:
            emit_source_load_particles_417ca0(platform);
            return;
        case VehicleParticleChild::landing:
            emit_source_landing_particles_416f30(platform);
            return;
        case VehicleParticleChild::surface:
            emit_source_surface_particles_417800(platform);
            return;
        case VehicleParticleChild::launch_wheel:
            emit_source_launch_wheel_particles_4173e0(platform);
            return;
        }
        throw FormatError("particle stage child outside 418830/418970");
    }
};
} // namespace
SourceNativeVehicleUpdate::SourceNativeVehicleUpdate(RaceSessionPlatform &platform)
    : platform_(platform), audio_(platform)
{
    platform_.require_owner_thread();
    if (!platform_.source_construction_only_entry())
        throw FormatError("native vehicle update requires the single source platform");
}
void SourceNativeVehicleUpdate::perform(SourceCoreRaceUpdateChild child, FrontendState &frontend,
                                        RaceTextureReplacement &textures)
{
    platform_.require_owner_thread();
    if (frontend.shared_platform().get() != &platform_ || &frontend.world() != &platform_.state())
        throw SourceAllocationError(0x44285a, "vehicle update requires the retained frontend/platform world");
    if (child == SourceCoreRaceUpdateChild::vehicles)
    { // 44285A ->418A90
        auto &s = platform_.state();
        const SourceTrackSplines splines{frontend.shared_platform()};
        const auto reset = source_vehicle_reset_parameters(s, splines); // call-scoped spline view
        const auto view = projection(s);
        SourceStripStage strip{platform_};
        SourceParticleStage particles{platform_};
        const RaceSessionVehicleResources resources{platform_.source_scene_resources().models(),
                                                    splines,
                                                    collision_.get(platform_),
                                                    audio_,
                                                    reset,
                                                    view,
                                                    nullptr,
                                                    &strip,
                                                    &particles};
        tick_session_vehicle_world(s, resources);
        return;
    }
    if (child == SourceCoreRaceUpdateChild::demo)
    {
        update_source_demo_40de30(frontend);
        return;
    } // 44286E ->40DE30
    remaining_.perform(child, frontend, textures);
}
std::uint32_t SourceNativeVehicleUpdate::contains_4278a0(const AudioEnvironmentRegion &region,
                                                         const CameraVector &point)
{
    platform_.require_owner_thread();
    return remaining_.contains_4278a0(region, point);
}
} // namespace sgn::retail
