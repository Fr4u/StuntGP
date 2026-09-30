#include "sgn/race_session_vehicle_world.hpp"
#include "sgn/contact_landing_particles.hpp"
#include "sgn/contact_load_particles.hpp"
#include "sgn/contact_surface_particles.hpp"
#include "sgn/contact_tire_particles.hpp"
#include "sgn/contact_vehicle_particles.hpp"
#include "sgn/spline_resolution.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
float gameplay(const RaceSessionState &s, unsigned address)
{
    return s.gameplay.values.at((address - 0x60f2d0) / 4);
}
std::string_view text(const RaceSessionState &s, unsigned address)
{
    const auto value = s.game_text.text((address - 0x6127a0) / 4);
    if (!value)
        throw FormatError("vehicle world requires source game text");
    return *value;
}
Vec3 unit_gravity(const RaceSessionState &s)
{
    return {s.gravity.unit.x, s.gravity.unit.y, s.gravity.unit.z};
}
std::array<std::string_view, 6> names(const RaceSessionState &s)
{
    std::array<std::string_view, 6> result;
    for (unsigned j = 0; j < 6; ++j)
    {
        const auto &name = s.cars[j].name;
        const auto end = std::find(name.begin(), name.end(), '\0');
        if (end == name.end())
            throw FormatError("vehicle world name is not terminated");
        result[j] = {name.data(), static_cast<std::size_t>(end - name.begin())};
    }
    return result;
}
VehicleTrailProjection projection(const RaceSessionState &s)
{
    if (s.controls.current >= 4)
        throw FormatError("vehicle world current controller");
    const auto &v = s.viewport;
    return {s.frame_world_view,
            s.frame_projection[0],
            s.frame_projection[5],
            v.center_x,
            v.center_y,
            v.scale_x,
            v.scale_y,
            s.camera_lifecycle[s.controls.current].clip_near};
}
VehicleResetParameters reset_parameters(const RaceSessionState &s, const VehicleResetParameters &immutable)
{
    auto result = immutable;
    result.stage = as_signed(s.clock.phase);
    result.mode = as_signed(s.clock.game_mode);
    result.selector = as_signed(s.level.game_type);
    result.normal_energy_cost = gameplay(s, 0x60f2d4);
    result.upgraded_energy_cost = gameplay(s, 0x60f2dc);
    result.course_length = s.clock.course_length;
    result.mask = s.curves.respawn;
    result.unit_gravity = s.gravity.unit;
    result.tick = s.simulation_tick;
    result.vehicle_names = names(s);
    result.special_end_text = text(s, 0x6127fc);
    result.retired_text = text(s, 0x6127e4);
    return result;
}
struct Audio final : VehicleScoreAudio, VehicleStuntChainAudio
{
    AudioDriver driver;
    Audio(RaceSessionState &s, const RaceSessionVehicleResources &r)
        : driver{s.audio_driver, r.audio_device, s.audio_level, s.controls, s.split_setting, r.audio_observer}
    {
    }
    void play(std::uint32_t id, std::uint32_t volume) override
    {
        audio_start(driver, id, 0, 0, volume, 255, 0, nullptr);
    }
    void play_chain(std::uint32_t id) override
    {
        audio_start(driver, id, 0, 0, 255, 0, 1, nullptr);
    }
};
VehicleStuntContext stunt_context(const RaceSessionState &s, Audio &audio)
{
    return {as_signed(s.clock.phase), as_signed(s.clock.game_mode),
            as_signed(s.score_block), as_signed(s.hud_block),
            s.simulation_tick,        s.score_rules.values.at((0x60d708 - 0x60d688) / 4),
            text(s, 0x61284c),        audio};
}
VehicleStuntRewards rewards(const RaceSessionState &s)
{
    VehicleStuntRewards result;
    std::copy_n(s.score_rules.values.begin(), result.points.size(), result.points.begin());
    for (unsigned j = 1; j < result.messages.size(); ++j)
        result.messages[j] = text(s, 0x6127a0 + j * 4);
    result.normal_energy = gameplay(s, 0x60f2d8);
    result.upgraded_energy = gameplay(s, 0x60f2e0);
    return result;
}
struct Inversion final : contact::surface::InvertedContact
{
    RaceSessionState &session;
    const VehicleStuntContext &context;
    Inversion(RaceSessionState &s, const VehicleStuntContext &c) : session(s), context(c) {}
    void apply(VehicleState &car, contact::player::Controls &controls, contact::player::VehicleId id) override
    {
        update_inversion_state(car, controls, id, session.score_popups, context, unit_gravity(session));
    }
};
std::array<VehicleAudioCar, 6> audio_cars(VehicleStorage &s)
{
    const auto at = [&](unsigned j) {
        return VehicleAudioCar{s.parameters[j], s.physics[j], s.race[j], s.contact[j], s.load[j], s.audio[j], j};
    };
    return {{at(0), at(1), at(2), at(3), at(4), at(5)}};
}
} // namespace

void tick_session_replay_visuals(RaceSessionState &s, const RaceSessionVehicleResources &r, VehicleTickAudit *audit)
{
    const auto current = s.current_slot;
    if (current >= 6)
        throw FormatError("replay visual current vehicle");
    auto &v = s.vehicles;
    auto &physical = v.physics[current];
    const auto &parameter = v.parameters[current];
    auto &presentation = v.presentation[current];
    auto &effect = v.model_effects[current];
    if (!v.models[current].model)
        throw FormatError("replay visual missing authored model");
    (void)r.models.source_backed(*v.models[current].model);
    Audio audio(s, r);
    auto views = audio_cars(v);
    VehicleAudioWorld vehicle_audio{views, audio.driver, s.controls, s.vehicle_audio, s.audio_frontier, s.pause_flag};
    auto trail_projection = projection(s);
    const auto settings = vehicle_trail_settings(s.trail_profiles);
    const auto first = resolve_spline(r.splines.first(), 0x418970);
    if (!first)
        throw FormatError("replay visual missing spline descriptors"); // R18 [624474] descriptor 0
    const VehicleRoutePresentation route{s.curves.lights, s.curves.ambience, s.curves.ambience_adjacent_word,
                                         first->descriptor.point_count};
    const auto trace = [&](unsigned address) {
        if (audit && audit->before_call)
            audit->before_call(audit->call_context, address);
    };
    auto *frames = audit ? audit->particle_frames : nullptr;
    auto *payload = audit ? audit->contact_payload : nullptr;
    trace(0x418970);
    update_vehicle_audio(vehicle_audio, current);
    trace(0x418975);
    engage_clutch(parameter, physical);
    trace(0x41897a);
    update_vehicle_brake_lights(physical, v.trail_inputs[current], effect, r.models, *v.models[current].model);
    trace(0x41897f);
    update_vehicle_route_lights(physical, v.race[current], v.trail_inputs[current], effect, r.models,
                                *v.models[current].model, route);
    trace(0x418984);
    update_turbo(parameter, physical);
    trace(0x418989);
    suspension_pose(parameter, physical);
    trace(0x41898e);
    locate_vehicle_wheels(parameter, physical, presentation, audit ? audit->wheel_positions : nullptr);
    trace(0x418993);
    update_vehicle_brake_effects(physical, r.models, *v.models[current].model);
    trace(0x418998);
    update_vehicle_launch_effect(
        VehicleLaunchBindings{physical, s.model_basis.borrow_retaining_basis_w(0x418998), trail_projection,
                              s.impact_particles.rate_selector, audit ? audit->trail_writes : nullptr},
        effect, v.anchors[current], r.models, *v.models[current].model, s.impact_particles, s.random);
    if (auto *stage = r.particle_stage)
    { // R21: source-world particle children, original order
        using C = VehicleParticleChild;
        trace(0x41899d);
        stage->emit(C::load, current);
        trace(0x4189a2);
        stage->emit(C::landing, current);
        trace(0x4189a7);
        stage->emit(C::surface, current);
        trace(0x4189ac);
        stage->emit(C::launch_wheel, current);
        trace(0x4189b1);
        stage->emit(C::engine, current);
        trace(0x4189b6);
        stage->emit(C::tire, current);
    }
    else
    {
        trace(0x41899d);
        contact::effects::emit_load_particles(s.impact_particles, s.random, physical, v.contact[current],
                                              v.particles[current], v.audio[current], s.gravity.magnitude, payload);
        trace(0x4189a2);
        contact::effects::emit_landing_wheel_particles(physical, presentation, s.gravity, s.random, s.effect_particles,
                                                       s.model_draws.template_pose, frames);
        trace(0x4189a7);
        contact::effects::emit_surface_particles(physical, v.contact[current], presentation, v.particles[current],
                                                 s.gravity, s.random, s.effect_particles, s.model_draws.template_pose,
                                                 frames);
        trace(0x4189ac);
        contact::effects::emit_launch_wheel_particles(physical, presentation, s.impact_particles.rate_selector,
                                                      s.random, s.effect_particles, frames);
        trace(0x4189b1);
        contact::effects::emit_engine_particles(parameter, physical, v.particles[current], s.gravity, r.models,
                                                *v.models[current].model, s.effect_staging, s.random,
                                                s.effect_particles, s.model_draws.template_pose, frames);
        trace(0x4189b6);
        contact::effects::emit_tire_particles(physical, presentation, v.particles[current], s.gravity, s.random,
                                              s.effect_particles, s.model_draws.template_pose, frames);
    }
    trace(0x4189bb); // R18 A7: a bound strip stage is the source-bound 41D140 owner
    if (r.strip_stage)
        r.strip_stage->update(current);
    else
        update_vehicle_skid(physical, v.race[current], presentation, s.replays, s.impact_particles.rate_selector,
                            v.strips[current], v.skid_rings[current], s.strip, audit ? audit->skid : nullptr);
    trace(0x4189c0);
    update_vehicle_speed_trail(VehicleSpeedTrailBindings{physical, parameter, v.trails[current],
                                                         s.model_basis.borrow_retaining_basis_w(0x4189c0),
                                                         trail_projection, settings, s.impact_particles.rate_selector,
                                                         audit ? audit->trail_writes : nullptr},
                               r.models, *v.models[current].model);
    trace(0x4189c5);
    update_vehicle_pair_a_trails(
        VehiclePairTrailBindings{physical, v.trails[current], s.model_basis.borrow_retaining_basis_w(0x4189c5),
                                 v.trail_inputs[current], trail_projection, settings, s.impact_particles.rate_selector,
                                 audit ? audit->trail_writes : nullptr},
        r.models, *v.models[current].model);
    if (presentation.field_3924 == 1)
    { // 4189CF..4189D8 conditional tail call
        const contact::Vector4 inactive{};
        const auto lighting = shadow_lighting(s.lights, s.light_selection);
        if (s.light_selection.active_count && !lighting)
            throw FormatError("replay visual active light has no PMD owner");
        const auto shadow = lighting ? *lighting : VehicleShadowLighting{s.light_selection.active_count, inactive};
        trace(0x4189d8);
        update_vehicle_shadow(physical, presentation, v.shadows[current], v.models[current].shadow.has_value(), shadow,
                              s.impact_particles.rate_selector, current, s.controls, unit_gravity(s), r.geometry,
                              s.contact_workspace, s.contact_cast, audit ? audit->cast : nullptr);
    }
}

namespace
{
class SessionSteps final : public WorldSteps, public RankingAnnouncements
{
    RaceSessionState &s;
    const RaceSessionVehicleResources &resources;
    VehicleWorldTickAudit *audit;
    VehicleTickAudit *vehicle_audit() const
    {
        return audit ? audit->vehicle : nullptr;
    }
    void stage(unsigned entry, unsigned current) const
    {
        if (s.current_slot != current)
            throw FormatError("vehicle world traversal owner mismatch");
        if (audit && audit->before_stage)
            audit->before_stage(audit->context, entry, current);
    }

  public:
    SessionSteps(RaceSessionState &state, const RaceSessionVehicleResources &r, VehicleWorldTickAudit *a)
        : s(state), resources(r), audit(a)
    {
    }
    std::uint16_t replay_status(ReplayHandle handle) const override
    {
        return s.replays.status(handle);
    }
    void contact(unsigned current) override
    {
        stage(0x409b60, current);
        Audio audio(s, resources);
        const auto stunt = stunt_context(s, audio);
        Inversion inversion(s, stunt);
        contact::solver::Parameters p;
        p.gravity = s.gravity.magnitude;
        p.restitution = s.curves.restitution;
        p.energy = {gameplay(s, 0x60f334), gameplay(s, 0x60f338), gameplay(s, 0x60f33c),
                    gameplay(s, 0x60f340), gameplay(s, 0x60f344), gameplay(s, 0x60f2f4)};
        p.game_mode = as_signed(s.clock.phase);
        p.race_mode = as_signed(s.clock.game_mode);
        p.track_mode = as_signed(s.level.texture_suffix);
        auto cars = s.vehicles.contact_views();
        contact::solver::World world{cars,
                                     resources.geometry,
                                     p,
                                     s.controls,
                                     s.impact_particles,
                                     s.random,
                                     inversion,
                                     s.contact_workspace,
                                     s.contact_cast,
                                     vehicle_audit() ? vehicle_audit()->contact_payload : nullptr,
                                     vehicle_audit() ? vehicle_audit()->cast : nullptr};
        contact::solver::step(world, current);
    }
    void advance_replay(unsigned current) override
    {
        stage(0x41af50, current);
        Audio audio(s, resources);
        const auto stunt = stunt_context(s, audio);
        const auto points = rewards(s);
        VehicleAirTickContext air{
            s.controls, current, s.score_popups,     stunt,           points,
            s.random,   audio,   s.effect_particles, unit_gravity(s), s.model_draws.template_pose};
        auto &v = s.vehicles;
        const ReplayPlaybackVehicle car{v.parameters[current], v.physics[current], v.contact[current],
                                        v.wheels[current], v.air[current]};
        const ReplayPlaybackContext context{s.gravity.magnitude, s.dynamics_workspace, air};
        update_replay_vehicle(s.replays, v.race[current], s.replay_workspace, car, context);
    }
    void playback_pose(unsigned current) override
    {
        stage(0x418970, current);
        tick_session_replay_visuals(s, resources, vehicle_audit());
    }
    void vehicle_step(unsigned current) override
    {
        stage(0x418830, current);
        const auto reset = reset_parameters(s, resources.reset_parameters);
        const auto view = projection(s);
        const RaceSessionVehicleResources invocation{resources.models,
                                                     resources.splines,
                                                     resources.geometry,
                                                     resources.audio_device,
                                                     reset,
                                                     view,
                                                     resources.audio_observer,
                                                     resources.strip_stage,
                                                     resources.particle_stage};
        tick_session_vehicle(s, invocation, vehicle_audit());
    }
    void ranking() override
    {
        stage(0x41e2d0, s.current_slot);
        update_ranking(s.vehicles.race, s.ranking, names(s), text(s, 0x612800), *this);
    }
    void leader(unsigned current, std::string_view message) override
    {
        contact::player::post_notice(s.controls, as_signed(s.clock.phase), s.simulation_tick, 180, message, current, 1,
                                     100);
    }
};
} // namespace
void tick_session_vehicle_world(RaceSessionState &s, const RaceSessionVehicleResources &resources,
                                VehicleWorldTickAudit *audit)
{
    WorldTraversal traversal{s.vehicles.physics, s.vehicles.race, s.current_slot, s.clock.game_mode};
    SessionSteps steps(s, resources, audit);
    world_tick(traversal, steps);
}
} // namespace sgn::retail
