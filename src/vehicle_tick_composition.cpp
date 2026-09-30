#include "sgn/vehicle_tick_composition.hpp"
#include "sgn/vehicle_model_detail.hpp"
#include "sgn/retail_vehicle_storage.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/vehicle_tick_input.hpp"
#include "sgn/vehicle_challenge_progression.hpp"
#include "sgn/contact_landing_particles.hpp"
#include "sgn/contact_load_particles.hpp"
#include "sgn/contact_surface_particles.hpp"
#include "sgn/contact_tire_particles.hpp"
#include "sgn/contact_vehicle_particles.hpp"
#include "sgn/format_require.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
bool same_float(float a, float b)
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
bool same_vector(Vec3 a, Vec3 b)
{
    return same_float(a.x, b.x) && same_float(a.y, b.y) && same_float(a.z, b.z);
}
Vec3 xyz(VehicleVector v)
{
    return {v.x, v.y, v.z};
}
} // namespace
void validate_vehicle_tick_bindings(const VehicleTickWorld &w, const VehicleTickConfiguration &configuration)
{
    const auto current = w.rail.reset.current;
    const auto &c = w.progression;
    const auto &d = w.dynamics;
    require(current < 6 && c.current == current && d.current == current && d.air.vehicle_id == current,
            "tick current vehicle identities disagree");
    const bool trial = w.automatic_environment.game_type == 4 &&
                       (c.clock.game_mode == 1 || (c.clock.game_mode == 4 && c.clock.lap_count == 100));
    const bool normal = w.automatic_environment.game_type == 2 && (c.clock.game_mode == 1 || c.clock.game_mode == 2);
    const bool challenge = w.automatic_environment.game_type == 3 && c.clock.lap_count == 100 &&
                           (c.clock.game_mode == 1 || c.clock.game_mode == 3);
    require(trial || normal || challenge,
            "full tick requires supported TimeTrial, normal-race or challenge source mode");
    if (normal)
        require(w.rival_step && w.normal_progression, "normal race requires full rival AI and progression bindings");
    if (challenge)
        require(configuration.ground_rewards, "challenge tick requires source stunt rewards");
    if (c.clock.game_mode == 1 || c.clock.game_mode == 2)
    {
        require(w.prestart_finish && configuration.ground_rewards,
                "prestart tick requires finish world and source stunt rewards");
        const auto &finish = *w.prestart_finish;
        require(finish.physical.data() == w.vehicles.physics.data() && finish.race.data() == w.vehicles.race.data() &&
                    &finish.clock == &c.clock && &finish.records == &c.records,
                "prestart finish owners disagree");
        require(finish.hud_block == std::bit_cast<std::uint32_t>(d.air.stunt.hud_block),
                "prestart finish HUD flag disagrees");
    }
    require(&w.rail.reset.controls == &w.contact.controls && &c.controls == &w.contact.controls &&
                &d.air.controls == &w.contact.controls && &w.audio.controls == &w.contact.controls &&
                &w.audio.driver.controls == &w.contact.controls,
            "tick controller owners disagree");
    require(&w.rail.random == &w.contact.random && &d.air.random == &w.contact.random, "tick RNG owners disagree");
    if (w.player_prelude)
        require(&w.player_prelude->controls == &w.contact.controls && &w.player_prelude->random == &w.contact.random,
                "tick player prelude owners disagree");
    require(&w.rail.particles == &w.contact.particles && &w.rail.audio == &w.audio.driver,
            "tick particle/audio owners disagree");
    if (w.projection_globals)
        require(&w.projection_globals->shared_model_identity == &w.rail.shared_model_identity,
                "tick rail and projection must borrow the same61C468 owner");
    require(&w.contact.cast == &w.cast, "tick contact query owners disagree");
    require(&c.audio == &d.air.chain_audio && &c.replays == &w.replays, "tick progression shared owners disagree");
    require(w.rail.payload_audit == w.contact.audit, "tick contact payload audit owners disagree");
    require(c.tick == d.dynamics.tick && c.tick == d.air.stunt.tick && c.tick == w.rail.reset.parameters.tick,
            "tick source clocks disagree");
    require(c.clock.phase == std::bit_cast<std::uint32_t>(d.air.stunt.game_mode) &&
                c.clock.phase == std::bit_cast<std::uint32_t>(d.dynamics.engine.game_mode) &&
                c.clock.phase == std::bit_cast<std::uint32_t>(w.rail.reset.parameters.stage) &&
                c.clock.phase == configuration.route_assist.phase,
            "tick source stages disagree");
    require(c.clock.game_mode == std::bit_cast<std::uint32_t>(d.air.stunt.race_mode) &&
                c.clock.game_mode == std::bit_cast<std::uint32_t>(w.rail.reset.parameters.mode),
            "tick source modes disagree");
    const auto unit = xyz(w.gravity.unit);
    require(same_vector(c.unit_gravity, unit) && same_vector(d.air.unit_gravity, unit) &&
                same_vector(w.automatic_environment.world_up, unit) &&
                same_vector(xyz(w.rail.reset.parameters.unit_gravity), unit) &&
                same_vector(configuration.route_assist.unit_gravity, unit),
            "tick unit gravity sources disagree");
    require(same_vector(w.rail.configuration.gravity, w.gravity.vector) &&
                same_vector(d.dynamics.gravity, w.gravity.vector) &&
                same_float(d.dynamics.gravity_magnitude, w.gravity.magnitude) &&
                same_float(d.dynamics.inverse_gravity, w.gravity.inverse_magnitude),
            "tick gravity sources disagree");
    for (unsigned j = 0; j < 6; ++j)
    {
        const auto &v = w.contact.vehicles[j];
        const auto &reset = w.rail.reset.vehicles[j];
        const auto &audio = w.audio.cars[j];
        require(v.id == j && reset.id == j && audio.id == j, "tick physical slot IDs disagree");
        require(&v.parameters == &w.vehicles.parameters[j] && &v.state == &w.vehicles.physics[j] &&
                    &v.race == &w.vehicles.race[j] && &v.contact == &w.vehicles.contact[j],
                "tick contact vehicle owners disagree");
        require(&reset.physics == &w.vehicles.physics[j] && &reset.race == &w.vehicles.race[j] &&
                    &reset.contact == &w.vehicles.contact[j],
                "tick reset vehicle owners disagree");
        require(&audio.parameters == &w.vehicles.parameters[j] && &audio.physics == &w.vehicles.physics[j] &&
                    &audio.race == &w.vehicles.race[j] && &audio.contact == &w.vehicles.contact[j] &&
                    &audio.load == &w.vehicles.load[j] && &audio.audio == &w.vehicles.audio[j],
                "tick audio vehicle owners disagree");
        require(&d.world[j].physics == &w.vehicles.physics[j] && &d.world[j].race == &w.vehicles.race[j],
                "tick wake vehicle owners disagree");
        require(&w.rail.parameters[j] == &w.vehicles.parameters[j] && &w.rail.vehicles[j] == &w.vehicles.rail[j],
                "tick rail vehicle owners disagree");
    }
    require(w.vehicles.models[current].model.has_value(), "tick has no actual model resource");
    (void)w.models.source_backed(*w.vehicles.models[current].model);
}
VehicleTickObservation tick_time_trial_vehicle(VehicleTickWorld &w, const VehicleTickConfiguration &configuration,
                                               VehicleTickAudit *audit)
{
    validate_vehicle_tick_bindings(w, configuration);
    const auto trace = [&](std::uint32_t address) {
        if (audit && audit->before_call)
            audit->before_call(audit->call_context, address);
    };
    // PinnedD3D418830..418968. These callees do not change4D5BB8 on this domain;
    // all adapters have been checked against that one physical slot above.
    const auto current = w.rail.reset.current;
    auto &storage = w.vehicles;
    auto &physical = storage.physics[current];
    auto &race = storage.race[current];
    auto &parameter = storage.parameters[current];
    auto &contact = storage.contact[current];
    auto &presentation = storage.presentation[current];
    auto &controls = w.contact.controls;
    auto &random = w.contact.random;
    auto &impact = w.contact.particles;
    auto &particles = w.dynamics.air.particles;
    const auto &clock = w.progression.clock;
    const auto phase = std::bit_cast<std::int32_t>(clock.phase);
    const auto unit = xyz(w.gravity.unit);
    const VehicleTickInputContext input{
        w.progression.tick, phase, unit, controls, current, w.progression.names, configuration.low_energy_text};
    const VehicleStartBoostContext boost{std::bit_cast<std::int32_t>(clock.race_mode),
                                         phase,
                                         w.progression.tick,
                                         configuration.start_boost_score,
                                         configuration.start_boost_text,
                                         controls,
                                         current};
    const VehicleRouteAssistContext routes{w.automatic, storage.ai[current], storage.audio[current],
                                           storage.drivers[current].skill, configuration.route_assist};
    auto trail_projection = w.trail_projection;
    VehicleTickObservation result;
    if (physical.energy_exempt == 1 || race.stopped == 1)
        disable_vehicle_control(race, physical); // 41884B
    clear_accumulated_impact_loss(contact);      // 418850
    trace(0x418855);
    prepare_vehicle_presentation(presentation,
                                 vehicle_presentation_effects(storage.model_effects[current], storage.anchors[current]),
                                 w.models, *storage.models[current].model); // 418855
    prepare_physical_parameters(parameter, physical);                       // 41885A
    // 41885F ->419910 is the source's literal RET.
    trace(0x418864);
    dispatch_vehicle_input(parameter, physical, race, storage.tick[current], input); // 418864
    RailUpdateAudit rail_audit;
    trace(0x418869);
    update_rail(w.rail, &rail_audit); // 418869
    if (rail_audit.lens_controller)
    {
        // 419310->403310->44F4C0 writes the shared projection before the
        // later speed/paired trail procedures read61C3E8/61C3FC.
        const auto &camera = controls.controllers.at(*rail_audit.lens_controller).camera;
        trail_projection.projection_x = camera.projection[0];
        trail_projection.projection_y = camera.projection[5];
        if (w.projection_globals)
        {
            auto &globals = *w.projection_globals;
            globals.projection = camera.projection;
            globals.inverse_depth_scale = camera.inverse_depth_scale;
            globals.depth_offset = camera.depth_offset;
        }
    }
    update_vehicle_performance(physical, storage.restart[current], storage.tick[current], storage.race, current,
                               configuration.performance);                                        // 41886E
    update_vehicle_start_boost(parameter, physical, race, contact, storage.tick[current], boost); // 418873
    auto automatic_vehicles = storage.automatic_views();
    trace(0x418878);
    ai_automatic_tick(w.rail.splines, automatic_vehicles, current, w.automatic, w.automatic_environment,
                      w.player_prelude, w.rival_step); // 418878
    // 41887D/418882 ->419970/419960 are both literal RET instructions.
    trace(0x418887);
    if (clock.game_mode == 1)
        result.progression = tick_prestart_progression(physical, race, storage.progression[current],
                                                       storage.ai[current], w.progression, *w.prestart_finish);
    else if (clock.game_mode == 2)
        result.progression = tick_normal_progression(physical, race, storage.progression[current], storage.ai[current],
                                                     w.progression, *w.normal_progression);
    else if (clock.game_mode == 3)
        result.progression = tick_challenge_progression(physical, race, storage.progression[current],
                                                        storage.ai[current], w.progression);
    else
        result.progression = tick_time_trial_progression(physical, race, storage.progression[current],
                                                         storage.ai[current], w.progression); // 418887
    if (clock.game_mode != 3)
    {
        trace(0x418895);
        update_vehicle_com(parameter, physical, contact, unit);
    } // 41888C..418895
    if (clock.game_mode != 4)
    {
        const VehicleGroundStuntContext ground{controls, current, w.dynamics.air.score, w.dynamics.air.stunt,
                                               *configuration.ground_rewards};
        update_vehicle_balance(physical, ground);                        // 4188A3
        update_vehicle_flip(physical, ground);                           // 4188A8
        update_vehicle_drift(physical, ground);                          // 4188AD
        update_vehicle_slipstream(physical, ground);                     // 4188B2
        update_vehicle_roof_slide(physical, contact.shock_hold, ground); // 4188B7
    }
    trace(0x4188bc);
    update_vehicle_route_braking(parameter, physical, race, storage.route_assist[current], routes);  // 4188BC
    update_vehicle_route_guidance(parameter, physical, race, storage.route_assist[current], routes); // 4188C1
    approach_body_speed(physical);                                                                   // 4188C6
    const auto support = contact::solver::support(contact);
    update_support_basis(physical, support);                              // 4188CB
    animate_vehicle_wheels(parameter, physical, storage.wheels[current]); // 4188D0
    apply_gravity(physical.velocity, w.gravity.vector, 1.0f);             // 4188E5
    // 40E550@40E554..40E574 reads the selected controller's vehicle767C/7688.
    // Only after that guard does40E57A load the currently simulated vehicle.
    auto dynamics = w.dynamics.dynamics;
    const auto player = controls.controllers.at(controls.current).vehicle;
    const auto &player_race = storage.race.at(player);
    dynamics.engine.automatic_shift_allowed = player_race.automatic_drive == 1 || player_race.start_countdown < 61;
    const VehicleDynamicsTickContext dynamics_context{
        dynamics, w.dynamics.workspace, w.dynamics.world, current, w.dynamics.air, w.dynamics.upgrade_drag_multiplier};
    trace(0x4188ea);
    result.air = vehicle_dynamics_tick({parameter, physical, support, storage.air[current], storage.history[current]},
                                       dynamics_context);                                               // 4188EA
    idle_vehicle_suspension(physical, presentation, random);                                            // 4188EF
    locate_vehicle_wheels(parameter, physical, presentation, audit ? audit->wheel_positions : nullptr); // 4188F4
    ReplayObservation replay;
    for (unsigned j = 0; j < 4; ++j)
    {
        replay.wheel_angles[j] = storage.wheels[current][j].current_angle;
        replay.contact_loads[j] = contact.wheels[j].current_load;
    }
    replay.surface_type = contact.wheels[0].type;
    replay.effect_current = contact.shock;
    replay.effect_target = contact.pair_shock;
    replay.effect_active = contact.effect_2368;
    replay.effect_pending = std::bit_cast<std::uint32_t>(contact.shock_hold);
    trace(0x4188ff);
    record_vehicle(w.replays, race, physical, replay, w.gravity.magnitude, w.recording); // 4188FF
    trace(0x418904);
    update_vehicle_audio(w.audio, current); // 418904
    damp_auxiliary_motion(physical);        // 418909
    trace(0x41890e);
    update_vehicle_brake_lights(physical, storage.trail_inputs[current], storage.model_effects[current], w.models,
                                *storage.models[current].model); // 41890E
    update_vehicle_route_lights(physical, race, storage.trail_inputs[current], storage.model_effects[current], w.models,
                                *storage.models[current].model, configuration.route_presentation); // 418913
    update_vehicle_brake_effects(physical, w.models, *storage.models[current].model);              // 418918
    trace(0x41891d);
    update_vehicle_launch_effect(VehicleLaunchBindings{physical, w.model_basis, trail_projection, impact.rate_selector,
                                                       audit ? audit->trail_writes : nullptr},
                                 storage.model_effects[current], storage.anchors[current], w.models,
                                 *storage.models[current].model, impact, random); // 41891D
    auto *frame_audit = audit ? audit->particle_frames : nullptr;
    trace(0x418922);
    if (auto *stage = w.particle_stage)
    { // R21: source-world particle children, original order
        using C = VehicleParticleChild;
        stage->emit(C::engine, current);
        stage->emit(C::tire, current);
        stage->emit(C::load, current); // 418922/418927/41892C
        stage->emit(C::landing, current);
        stage->emit(C::surface, current);
        stage->emit(C::launch_wheel, current); // 418931/418936/41893B
    }
    else
    {
        contact::effects::emit_engine_particles(parameter, physical, storage.particles[current], w.gravity, w.models,
                                                *storage.models[current].model, w.effect_staging, random, particles,
                                                w.dynamics.air.template_pose, frame_audit); // 418922
        contact::effects::emit_tire_particles(physical, presentation, storage.particles[current], w.gravity, random,
                                              particles, w.dynamics.air.template_pose, frame_audit); // 418927
        contact::effects::emit_load_particles(impact, random, physical, contact, storage.particles[current],
                                              storage.audio[current], w.gravity.magnitude, w.contact.audit); // 41892C
        contact::effects::emit_landing_wheel_particles(physical, presentation, w.gravity, random, particles,
                                                       w.dynamics.air.template_pose, frame_audit); // 418931
        contact::effects::emit_surface_particles(physical, contact, presentation, storage.particles[current], w.gravity,
                                                 random, particles, w.dynamics.air.template_pose,
                                                 frame_audit); // 418936
        contact::effects::emit_launch_wheel_particles(physical, presentation, impact.rate_selector, random, particles,
                                                      frame_audit); // 41893B
    }
    trace(0x418940);
    update_vehicle_segments(parameter, physical, presentation, storage.anchors[current], storage.segments[current],
                            audit ? audit->rotation_matrix : nullptr); // 418940
    trace(0x418945);                                                   // 418945
    if (w.strip_stage)
        w.strip_stage->update(current); // R18 A7: source-bound strip owner (41D140)
    else
        update_vehicle_skid(physical, race, presentation, w.replays, impact.rate_selector, storage.strips[current],
                            storage.skid_rings[current], w.strip, audit ? audit->skid : nullptr);
    trace(0x41894a);
    update_vehicle_speed_trail(VehicleSpeedTrailBindings{physical, parameter, storage.trails[current], w.model_basis,
                                                         trail_projection, w.trail_settings, impact.rate_selector,
                                                         audit ? audit->trail_writes : nullptr},
                               w.models, *storage.models[current].model); // 41894A
    update_vehicle_pair_a_trails(VehiclePairTrailBindings{physical, storage.trails[current], w.model_basis,
                                                          storage.trail_inputs[current], trail_projection,
                                                          w.trail_settings, impact.rate_selector,
                                                          audit ? audit->trail_writes : nullptr},
                                 w.models, *storage.models[current].model); // 41894F
    update_vehicle_pair_b_trails(VehiclePairTrailBindings{physical, storage.trails[current], w.model_basis,
                                                          storage.trail_inputs[current], trail_projection,
                                                          w.trail_settings, impact.rate_selector,
                                                          audit ? audit->trail_writes : nullptr},
                                 w.models, *storage.models[current].model); // 418954
    trace(0x418959);
    update_vehicle_shadow(physical, presentation, storage.shadows[current], storage.models[current].shadow.has_value(),
                          w.shadow_lighting, impact.rate_selector, current, controls, unit, w.contact.geometry,
                          w.contact.workspace, w.cast, audit ? audit->cast : nullptr); // 418959
    check_vehicle_retirement(w.rail.reset); // 41895E; source retirement body is mode3-only.
    check_stuck_vehicle(physical, race);    // 418963
    trace(0x418968);
    result.bounds_placement =
        finish_vehicle_bounds(w.rail.reset, configuration.reset_bounds); // 418968 mandatory tail JMP.
    return result;
}
} // namespace sgn::retail
