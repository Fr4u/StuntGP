#include "sgn/race_session_world_tick.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/score_popup_lifetime.hpp"

namespace sgn::retail
{
namespace
{
VehicleTrailProjection projection(const RaceSessionState &s)
{
    if (s.controls.current >= s.camera_lifecycle.size())
        throw FormatError("world tick current controller outside camera owners");
    return {s.frame_world_view,    s.frame_projection[0],
            s.frame_projection[5], s.viewport.center_x,
            s.viewport.center_y,   s.viewport.scale_x,
            s.viewport.scale_y,    s.camera_lifecycle[s.controls.current].clip_near};
}
} // namespace
void tick_session_world(RaceSession &session, RaceTextureReplacement &textures,
                        std::span<const contact::effects::EffectAnimationFrames> animations,
                        SessionWorldTickAudit *audit)
{
    tick_session_world(session, {textures, session.audio_device(), animations}, audit);
}
void tick_session_world(RaceSession &session, SessionWorldDevices devices, SessionWorldTickAudit *audit)
{
    auto &s = session.state();
    const auto audio_view = [&] {
        return AudioDriver{s.audio_driver, devices.audio,   s.audio_level,
                           s.controls,     s.split_setting, audit ? audit->audio : nullptr};
    };
    const auto stage = [&](std::uint32_t address, const auto &call) {
        if (audit && audit->before_stage)
            audit->before_stage(audit->context, address);
        call();
        if (audit && audit->after_stage)
            audit->after_stage(audit->context, address);
    };
    stage(0x405210, [&] {
        auto camera = camera_frame_world(session);
        camera_frame_before_world(camera);
    });
    stage(0x427140, [&] {
        const auto model_handle = session.scene_resources().model_handle(3);
        advance_track_part_animations(s.track_part_animations ? s.track_part_animations->records()
                                                              : std::span<TrackPartAnimation>{},
                                      session.models(), model_handle);
    });
    stage(0x45e1d0, [&] {
        contact::effects::update_effect_particles(s.effect_particles, s.gravity.vector, devices.effect_animations);
    });
    stage(0x415df0, [&] {
        const auto view = projection(s);
        const auto profiles = impact_trail_parameters(s.trail_profiles);
        contact::effects::update_impact_particles({s.impact_particles, s.impact_draw_list, s.contact_workspace,
                                                   s.contact_cast, s.random, session.track_geometry(), s.gravity.vector,
                                                   view, profiles, audit ? audit->contact_payload : nullptr,
                                                   audit ? audit->impact : nullptr});
    });
    stage(0x443a80, [&] {
        auto audio = audio_view();
        (void)tick_session_lifecycle(session, devices.textures, audio);
    });
    stage(0x418a90, [&] {
        const auto reset = session.reset_parameters();
        const auto view = projection(s);
        // R18: preview START path keeps its decoded file form until WP-F retirement.
        const DecodedTrackSplines splines{session.splines(), session.raw_texture_labels(), session.track_pmd()};
        const RaceSessionVehicleResources resources{session.models(),
                                                    splines,
                                                    session.track_geometry(),
                                                    devices.audio,
                                                    reset,
                                                    view,
                                                    audit ? audit->audio : nullptr};
        tick_session_vehicle_world(s, resources, audit ? audit->vehicle : nullptr);
    });
    stage(0x401770, [&] {
        auto audio = audio_view();
        update_score_popups(s.score_popups, s.random, s.gravity.vector, audio);
    });
}
} // namespace sgn::retail
