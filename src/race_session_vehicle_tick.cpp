#include "sgn/race_session_vehicle_tick.hpp"
#include "sgn/vehicle_ai_rival.hpp"
#include "sgn/spline_resolution.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>

namespace sgn::retail
{
namespace
{
struct RivalStep final : AiRivalStep
{
    AiRivalWorld &world;
    explicit RivalStep(AiRivalWorld &value) : world(value) {}
    void tick(unsigned current) override
    {
        update_rival_ai(world, current);
    }
};
using sgn::bit_reinterpret::as_signed;
float gameplay(const RaceSessionState &s, unsigned address)
{
    return s.gameplay.values.at((address - 0x60f2d0) / 4);
}
std::uint32_t score(const RaceSessionState &s, unsigned address)
{
    return s.score_rules.values.at((address - 0x60d688) / 4);
}
std::string_view text(const RaceSessionState &s, unsigned address)
{
    const auto value = s.game_text.text((address - 0x6127a0) / 4);
    if (!value)
        throw FormatError("vehicle tick requires the authored game text");
    return *value;
}
struct StuntAudio final : VehicleScoreAudio, VehicleStuntChainAudio
{
    AudioDriver &driver;
    explicit StuntAudio(AudioDriver &value) : driver(value) {}
    void play(std::uint32_t id, std::uint32_t volume) override
    {
        audio_start(driver, id, 0, 0, volume, 255, 0, nullptr);
    }
    void play_chain(std::uint32_t id) override
    {
        audio_start(driver, id, 0, 0, 255, 0, 1, nullptr);
    }
};
struct Inversion final : contact::surface::InvertedContact
{
    VehicleScoreState &score_state;
    const VehicleStuntContext &context;
    Vec3 gravity;
    Inversion(VehicleScoreState &score_value, const VehicleStuntContext &c, Vec3 g)
        : score_state(score_value), context(c), gravity(g)
    {
    }
    void apply(VehicleState &vehicle, contact::player::Controls &controls, contact::player::VehicleId id) override
    {
        update_inversion_state(vehicle, controls, id, score_state, context, gravity);
    }
};
std::array<VehicleAudioCar, 6> audio_views(VehicleStorage &s)
{
    const auto at = [&](unsigned j) {
        return VehicleAudioCar{s.parameters[j], s.physics[j], s.race[j], s.contact[j], s.load[j], s.audio[j], j};
    };
    return {{at(0), at(1), at(2), at(3), at(4), at(5)}};
}
} // namespace

VehicleTickObservation tick_session_vehicle(RaceSessionState &s, const RaceSessionVehicleResources &resources,
                                            VehicleTickAudit *audit)
{
    if (s.current_slot >= 6 || s.controls.current >= 4)
        throw FormatError("session tick current identity");
    // R18 (DESIGN A1): call-scoped views of [624474] descriptor 0, 428800(1)
    // and 571328. Declared before every context that borrows them.
    const auto first = resolve_spline(resources.splines.first(), 0x418830);
    const auto route = resolve_spline(resources.splines.find(1), 0x4081d0);
    const auto start = resolve_spline(s.grid.start_spline, 0x41e210);
    if (!first || !route || !start)
        throw FormatError("session tick requires authored race splines");
    const Vec3 unit{s.gravity.unit.x, s.gravity.unit.y, s.gravity.unit.z};
    AudioDriver audio{s.audio_driver, resources.audio_device, s.audio_level,
                      s.controls,     s.split_setting,        resources.audio_observer};
    StuntAudio stunt_audio(audio);
    VehicleStuntContext stunt{
        as_signed(s.clock.phase), as_signed(s.clock.game_mode), as_signed(s.score_block), as_signed(s.hud_block),
        s.simulation_tick,        score(s, 0x60d708),           text(s, 0x61284c),        stunt_audio};
    VehicleStuntRewards rewards;
    std::copy_n(s.score_rules.values.begin(), rewards.points.size(), rewards.points.begin());
    // Index0 is the source null sentinel. Every live message index is required.
    for (unsigned j = 1; j < rewards.messages.size(); ++j)
        rewards.messages[j] = text(s, 0x6127a0 + j * 4);
    rewards.normal_energy = gameplay(s, 0x60f2d8);
    rewards.upgraded_energy = gameplay(s, 0x60f2e0);
    VehicleAirTickContext air{s.controls, s.current_slot, s.score_popups,     stunt, rewards,
                              s.random,   stunt_audio,    s.effect_particles, unit,  s.model_draws.template_pose};
    Inversion inversion(s.score_popups, stunt, unit);
    contact::solver::Parameters contact_parameters;
    contact_parameters.gravity = s.gravity.magnitude;
    contact_parameters.restitution = s.curves.restitution;
    contact_parameters.energy = {gameplay(s, 0x60f334), gameplay(s, 0x60f338), gameplay(s, 0x60f33c),
                                 gameplay(s, 0x60f340), gameplay(s, 0x60f344), gameplay(s, 0x60f2f4)};
    contact_parameters.game_mode = as_signed(s.clock.phase);
    contact_parameters.race_mode = as_signed(s.clock.game_mode);
    contact_parameters.track_mode = as_signed(s.level.texture_suffix);
    auto contacts = s.vehicles.contact_views();
    auto *payload = audit ? audit->contact_payload : nullptr;
    contact::solver::World contact{
        contacts,  resources.geometry,  contact_parameters, s.controls, s.impact_particles,           s.random,
        inversion, s.contact_workspace, s.contact_cast,     payload,    audit ? audit->cast : nullptr};
    auto resets = s.vehicles.reset_views();
    VehicleResetWorld reset{resets, s.current_slot, s.controls, s.reset_globals, resources.reset_parameters};
    RailParameters rail_parameters{gameplay(s, 0x60f2e4), gameplay(s, 0x60f2e8), s.gravity.vector, text(s, 0x6127f4)};
    RailWorld rail{reset,           s.vehicles.parameters, s.vehicles.rail, s.rail, resources.splines,
                   rail_parameters, s.impact_particles,    s.random,        audio,  s.shared_model_identity,
                   payload};
    std::array<std::string_view, 6> names;
    for (unsigned j = 0; j < 6; ++j)
    {
        const auto &name = s.cars[j].name;
        const auto end = std::find(name.begin(), name.end(), '\0');
        if (end == name.end())
            throw FormatError("session vehicle name is not terminated");
        names[j] = {name.data(), static_cast<std::size_t>(end - name.begin())};
    }
    VehicleProgressionContext progression{s.clock,
                                          s.records,
                                          start,
                                          unit,
                                          s.controls,
                                          s.current_slot,
                                          s.simulation_tick,
                                          s.trial_selection.availability[1],
                                          s.ghosts,
                                          s.replays,
                                          names,
                                          text(s, 0x612804),
                                          text(s, 0x612808),
                                          text(s, 0x612814),
                                          text(s, 0x612818),
                                          stunt_audio};
    const VehicleDynamicsContext dynamics{
        {as_signed(s.clock.phase), false, gameplay(s, 0x60f2f8), gameplay(s, 0x60f2fc)},
        s.gravity.magnitude,
        s.gravity.inverse_magnitude,
        s.gravity.vector,
        0,
        s.simulation_tick};
    auto wakes = s.vehicles.wake_views();
    const VehicleDynamicsTickContext dynamics_tick{dynamics, s.dynamics_workspace, wakes, s.current_slot,
                                                   air,      gameplay(s, 0x60f308)};
    const AiAutomaticEnvironment automatic{unit, gameplay(s, 0x60f2ec), gameplay(s, 0x60f2f0), s.level.game_type};
    auto audio_cars = audio_views(s.vehicles);
    VehicleAudioWorld vehicle_audio{audio_cars, audio, s.controls, s.vehicle_audio, s.audio_frontier, s.pause_flag};
    // No position is read when the original active-count guard returns. A
    // populated view must have the actual selected PMD light, never a fallback.
    const contact::Vector4 inactive_light{};
    auto lighting = shadow_lighting(s.lights, s.light_selection);
    if (s.light_selection.active_count != 0 && !lighting)
        throw FormatError("session tick active light has no selected source record");
    const VehicleShadowLighting shadow =
        lighting ? *lighting : VehicleShadowLighting{s.light_selection.active_count, inactive_light};
    VehicleRaceFinishWorld finish{s.vehicles.physics, s.vehicles.race, s.ranking, s.finish, s.clock,
                                  s.records,          s.hud_block};
    VehicleTickProjectionGlobals projection{s.frame_projection, s.inverse_depth_scale, s.depth_offset,
                                            s.shared_model_identity};
    const AiPlayerPrelude player{s.controls, s.cars, s.random};
    auto rival_cars = s.vehicles.automatic_views();
    const AiRivalSettings rival_settings{gameplay(s, 0x60f318), gameplay(s, 0x60f31c), gameplay(s, 0x60f320),
                                         gameplay(s, 0x60f324), gameplay(s, 0x60f300), start->descriptor.length};
    AiRivalWorld rivals{rival_cars,
                        s.vehicles.contact,
                        s.vehicles.drivers,
                        s.vehicles.restart,
                        s.vehicles.tick,
                        s.vehicles.ai_stunts,
                        s.ranking,
                        s.clock,
                        s.rail,
                        s.stunt_knots,
                        s.automatic,
                        automatic,
                        rival_settings,
                        s.random,
                        s.simulation_tick};
    RivalStep rival_step(rivals);
    NormalRaceProgression normal{
        finish, s.vehicles.progression, s.split_setting, text(s, 0x612858), text(s, 0x612810), text(s, 0x61280c)};
    const auto trail_settings = vehicle_trail_settings(s.trail_profiles);
    // R18 (DESIGN A4): 4D5B30 borrowed with its retained W lanes 3/7.
    VehicleTickWorld world{s.vehicles,
                           contact,
                           rail,
                           progression,
                           dynamics_tick,
                           s.automatic,
                           automatic,
                           vehicle_audio,
                           resources.models,
                           s.selected_lod,
                           s.replays,
                           s.replay_workspace,
                           s.gravity,
                           s.model_basis.borrow_retaining_basis_w(0x418830),
                           resources.trail_projection,
                           trail_settings,
                           s.strip,
                           s.effect_staging,
                           shadow,
                           s.contact_cast,
                           &finish,
                           &projection,
                           &player,
                           &rival_step,
                           &normal,
                           resources.strip_stage,
                           resources.particle_stage};
    const VehiclePerformanceSettings performance{route->descriptor.length, gameplay(s, 0x60f318), gameplay(s, 0x60f328),
                                                 gameplay(s, 0x60f32c), gameplay(s, 0x60f330)};
    const VehicleRouteAssistSettings route_assist{s.clock.phase, unit, gameplay(s, 0x60f348)};
    const VehicleResetBounds bounds{s.curves.width_left, s.curves.width_right};
    VehicleGroundStuntRewards ground;
    constexpr std::array<unsigned, 10> points{0x60d718, 0x60d714, 0x60d710, 0x60d70c, 0x60d6f0,
                                              0x60d6dc, 0x60d6e0, 0x60d6e8, 0x60d6e4, 0x60d6f4};
    constexpr std::array<unsigned, 10> messages{0x612860, 0x61285c, 0x612868, 0x612864, 0x612880,
                                                0x6128cc, 0x6128c8, 0x612900, 0x612904, 0x6128d0};
    for (unsigned j = 0; j < ground.values.size(); ++j)
        ground.values[j] = {score(s, points[j]), text(s, messages[j])};
    const VehicleTickConfiguration configuration{
        performance,
        route_assist,
        bounds,
        {s.curves.lights, s.curves.ambience, s.curves.ambience_adjacent_word, first->descriptor.point_count},
        score(s, 0x60d6ec),
        text(s, 0x61287c),
        text(s, 0x6127f8),
        &ground};
    const auto stepped_slot = s.current_slot;
    const auto result = tick_time_trial_vehicle(world, configuration, audit);
    if (result.bounds_placement)
        ++s.presentation_vehicle_continuity[stepped_slot];
    return result;
}
} // namespace sgn::retail
