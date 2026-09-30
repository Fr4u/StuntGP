#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/retail_game_text_table.hpp"
#include "sgn/retail_level_lighting.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/retail_track_part_initialization.hpp"
#include "sgn/retail_world_sprite_model.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
std::string_view text_bytes(std::span<const std::uint8_t> b)
{
    return {reinterpret_cast<const char *>(b.data()), b.size()};
}
std::uint32_t word(std::span<const std::uint8_t> b, std::size_t at)
{
    if (at > b.size() || b.size() - at < 4)
        throw FormatError("session PMD word outside resource");
    std::uint32_t v;
    std::memcpy(&v, b.data() + at, 4);
    return v;
}
template <std::size_t N> std::string_view bounded_text(const std::array<char, N> &a)
{
    const auto end = std::find(a.begin(), a.end(), '\0');
    if (end == a.end())
        throw FormatError("session name lacks terminator");
    return {a.data(), static_cast<std::size_t>(end - a.begin())};
}
void publish_level(RaceSessionState &s, const LevelOverrideState &p)
{
    const auto &l = p.level;
    s.level = {l.game_type,    l.music_track,     l.texture_suffix, l.rolling_start_offset, l.camera_mode,
               l.overlay_demo, l.demo_loop_ticks, l.ground_level,   l.track_texture};
    s.clock.countdown_pending = p.start_delay;
    s.clock.race_mode = p.race_mode;
    s.clock.remaining_ticks = l.time_ticks;
    s.clock.lap_count = l.lap_count;
    s.records = {p.record_effc, l.lap_record_ticks, p.record_f008};
    s.finish.best_total_ticks = p.record_f004;
    s.finish.best_total_score = p.record_f00c;
    s.audio_level = p.audio;
    s.split_setting = p.split;
    s.impact_particles.rate_selector = p.render_a;
    s.render_b = p.render_b;
    s.selected_camera_preset = p.display_selector;
    s.demo_ticks = p.demo_ticks;
    s.hud_block = p.demo_active;
}
} // namespace
void configure_session_audio(RaceSessionState &s, const RaceSessionLaunch &launch)
{
    // Full460C20 scalar producer, with explicit new frontend inputs in place of
    // the external boot CFG. Preserve ignored percentages exactly as the source.
    auto &level = s.audio_level;
    if (launch.audio_master_percent < 101)
        level.scale = launch.audio_master_percent;
    if (launch.sound_percent < 101)
        level.output_b = launch.sound_percent;
    if (launch.music_percent < 101)
        level.output_a = launch.music_percent;
    level.input_b = level.output_b;
    level.input_a = level.output_a;
    s.audio_initialization.requested_rate = std::clamp(launch.audio_rate, 5512u, 44100u);
    update_level_audio(level, s.options);
}
struct RaceSession::Resources
{
    std::shared_ptr<RaceSessionPlatform> platform;
    RaceSessionReadiness readiness{RaceSessionReadiness::source_attachment_only};
    struct PreviewResources
    {
        RaceSessionPlatform &platform;
        const std::filesystem::path &root;
        const AssetStore &assets;
        SceneResources scene;
        WindowsAudioDevice &device;
        WindowsAudioStreamPlatform &stream_platform;
        std::string level_name;
        std::uint64_t window{};
        Bytes pmd;
        TrackNormalAllocation track_normals;
        std::unique_ptr<SplineSet> splines;
        contact::TrackGeometry geometry;
        std::vector<std::string> raw_texture_labels;
        PreviewResources(RaceSessionPlatform &owner, const RaceSessionLaunch &launch)
            : platform(owner), root(owner.retail_root()), assets(owner.assets()), scene(assets, root),
              device(owner.audio_device()), stream_platform(owner.audio_stream_platform()),
              level_name(launch.level_name), window(launch.window)
        {
        }
        ~PreviewResources()
        {
            // Also runs on a failed preview constructor with retained aliases.
            // Source-only attachment never creates this per-level owner.
            try
            {
                platform.require_owner_thread();
            }
            catch (...)
            {
                std::terminate();
            }
            auto &s = platform.state();
            try
            {
                close_audio_stream(s.audio_stream, s.audio_maintenance.music, device, stream_platform);
            }
            catch (...)
            { /* Native owners retain final cleanup responsibilities. */
            }
            stream_platform.cancel_pending_callbacks();
            detach_world_polygon_level(s.polygon_frame);
            for (auto &presentation : s.camera_presentation)
                presentation.spline.reset();
            s.grid.start_spline.reset();
            s.rail.spline.reset();
            s.automatic.routes.reset();
        }
    };
    // Declared after platform: level borrows are finalized while it is alive.
    std::unique_ptr<PreviewResources> preview;
    Resources(SourceColdAttachmentOnly, std::shared_ptr<RaceSessionPlatform> owner) : platform(std::move(owner))
    {
        if (!platform)
            throw FormatError("source session attachment requires a platform");
        platform->require_owner_thread();
        if (!platform->source_construction_only_entry())
            throw FormatError("source attachment cannot borrow a preview host entry");
    }
    static std::shared_ptr<RaceSessionPlatform> preview_platform(std::filesystem::path path, float gain,
                                                                 SourceApplicationLiteralData *literals)
    {
        if (literals)
            return std::make_shared<RaceSessionPlatform>(PreviewHostEntry{}, std::move(path), gain, *literals);
        return std::make_shared<RaceSessionPlatform>(PreviewHostEntry{}, std::move(path), gain);
    }
    Resources(PreviewHostEntry, std::filesystem::path path, const RaceSessionLaunch &launch,
              SourceApplicationLiteralData *literals)
        : platform(preview_platform(std::move(path), launch.master_gain, literals)),
          readiness(RaceSessionReadiness::preview_constructing),
          preview(std::make_unique<PreviewResources>(*platform, launch))
    {
    }
    ~Resources()
    {
        try
        {
            platform->require_owner_thread();
        }
        catch (...)
        {
            std::terminate();
        }
    }
};

RaceSession::RaceSession(SourceColdAttachmentOnly tag, std::shared_ptr<RaceSessionPlatform> platform)
    : resources_(std::make_unique<Resources>(tag, std::move(platform))),
      state_(resources_->platform, &resources_->platform->state())
{
    resources_->platform->claim_session_attachment();
}
RaceSession::RaceSession(std::filesystem::path root, const RaceSessionLaunch &launch)
    : RaceSession(PreviewHostEntry{}, std::move(root), launch)
{
}
RaceSession::RaceSession(PreviewHostEntry tag, std::filesystem::path root, const RaceSessionLaunch &launch)
    : RaceSession(tag, std::move(root), launch, nullptr)
{
}
RaceSession::RaceSession(PreviewHostEntry tag, std::filesystem::path root, const RaceSessionLaunch &launch,
                         SourceApplicationLiteralData &literals)
    : RaceSession(tag, std::move(root), launch, &literals)
{
}
RaceSession::RaceSession(PreviewHostEntry tag, std::filesystem::path root, const RaceSessionLaunch &launch,
                         SourceApplicationLiteralData *literals)
    : resources_(std::make_unique<Resources>(tag, std::move(root), launch, literals)),
      state_(resources_->platform, &resources_->platform->state())
{
    resources_->platform->claim_session_attachment();
    auto &r = *resources_->preview;
    auto &s = *state_;
    if (launch.level_name.empty() || launch.level_name.find_first_of("/\\:") != std::string::npos ||
        launch.player_name.empty() || launch.player_name.size() >= s.selection[0].name.size() ||
        launch.player_name.find('\0') != std::string::npos || launch.model < 1 || launch.model > 20 ||
        launch.control_kind > 4 || launch.render_quality > 100 || launch.width <= 0 || launch.height <= 0 ||
        launch.depth_bits != 32)
        throw FormatError("invalid native race selection");
    if (launch.mode != SessionMode::time_trial && launch.mode != SessionMode::normal_race &&
        launch.mode != SessionMode::challenge)
        throw FormatError("unsupported native session mode");
    // NEW preview host sequence retained for compatibility. The source-cold
    // attachment above never executes these choices or later scene children.
    s.random.state = launch.random_seed;
    s.control_kind = launch.control_kind;
    // 442030 invokes445FA0. Its445FE8 store overrides the PE's factor1 with4
    // before the real441F90 calculation and before cold vehicle wheel loads.
    s.gravity.factor = 4;
    initialize_world_gravity(s.gravity);
    initialize_legacy_driver_restriction(s.view_quality); // 42E6D0 initial scene restriction
    s.viewport.width = launch.width;
    s.viewport.height = launch.height;
    s.viewport.depth_bits = launch.depth_bits;
    initialize_viewport(s.viewport, s.split_setting);
    s.vehicle_audio = {100, 0.75f};
    s.load_selection.configuration_id = 1; // pinned472C74, before any40BEC0 load selection
    s.contact_cast.mode = 1;               // pinned4754A0
    s.audio_level.scale = 255;
    s.audio_level.output_b = 255;
    s.audio_level.output_b_copy = 255;
    s.audio_driver.spatial_mode = 1;
    s.audio_driver.spatial_template = audio_default_spatial_parameters();
    audio_set_range(s.audio_driver, 7500);
    configure_session_audio(s, launch);
    auto driver = audio_view();
    AudioStartupWorld audio{
        driver,   s.audio_maintenance, s.audio_stream, s.audio_initialization, s.audio_level, s.options, r.assets,
        r.device, r.stream_platform,   r.window};
    if (!initialize_audio(audio) && s.audio_level.input_b != 0)
        throw FormatError("native audio startup failed");
    load_game_text(r.assets, 1, s.game_text);
    ensure_score_configuration(r.assets, s.score_rules);
    ensure_gameplay_configuration(r.assets, s.gameplay);

    const auto script = text_bytes(r.assets.resource("config\\lvscript.cfg"));
    const auto block = find_level_block(script, r.level_name);
    if (!block)
        throw FormatError("selected level absent from original script");
    LevelOverrideState plan;
    apply_level_settings(*block, plan.level);
    plan.audio = s.audio_level;
    auto bindings = parse_level_asset_bindings(*block);
    const auto sky = std::find_if(bindings.requests.begin(), bindings.requests.end(),
                                  [](const auto &request) { return request.type == 2; });
    initialize_sky_texture_motion(s.texture_animation.sky, sky == bindings.requests.end()
                                                               ? std::nullopt
                                                               : std::optional<std::string_view>{sky->name});
    if (!bindings.track_name)
        throw FormatError("selected level has no track resource");
    const auto car_settings = parse_level_car_settings(*block);
    for (unsigned i = 0; i < 6; ++i)
        s.cars[i].settings = car_settings[i];
    auto &selected = s.selection[0];
    selected.kind = 2;
    selected.control = 0;
    std::copy(launch.player_name.begin(), launch.player_name.end(), selected.name.begin());
    selected.model = launch.model;
    selected.config = launch.configuration;
    selected.tyre = launch.tyre;
    selected.livery = launch.livery;
    if (launch.mode == SessionMode::normal_race)
    {
        // Explicit stock lineup of the new frontend, not a claim to reproduce
        // the championship's roster generator.43A2A0/43A780 map each selected
        // participant to kind2, humancontrol0 or AIcontrol1.43A4A0 publishes
        // stock model/config together; stock physics still comes from CFG.
        // 100/100 is the original42C9B0 family default skill/mode, not tuning.
        unsigned model = 1;
        for (unsigned i = 1; i < 6; ++i)
        {
            if (model == launch.model)
                ++model;
            auto &rival = s.selection[i];
            rival.kind = 2;
            rival.control = 1;
            rival.model = model;
            rival.config = model;
            rival.tyre = 0;
            rival.livery = 0;
            rival.ai_skill = 100;
            rival.ai_mode = 100;
            const auto name = "CAR" + std::to_string(model);
            std::copy(name.begin(), name.end(), rival.name.begin());
            ++model;
        }
    }
    // 43A910's Time Trial scalar choices. The new frontend selects the stock
    //  authored level/car; custom garage tuning and a previous user replay are absent.
    s.options.selection_phase = 1;
    s.options.render_a = launch.render_quality;
    s.options.game_type = launch.mode == SessionMode::time_trial ? 4u : launch.mode == SessionMode::challenge ? 3u : 2u;
    s.options.challenge_ticks = launch.challenge_ticks;
    s.options.race_mode = launch.mode == SessionMode::time_trial ? 3u : 1u;
    s.options.start_delay = 4;
    s.options.record_f004 = 36000;
    s.options.music = plan.level.music_track;
    s.options.texture = 8;
    s.options.laps = plan.level.lap_count;
    s.options.record = plan.level.lap_record_ticks;
    LevelSelectionDependencies selection({s.selection, s.cars, s.replays, s.ghosts, s.trial_selection, s.random,
                                          r.assets, bindings, s.vehicles, r.level_name,
                                          std::numeric_limits<std::uint32_t>::max()});
    apply_level_overrides(s.options, plan, selection);
    publish_level(s, plan);
    s.scene_mode = 1;
    reset_view_matrix_cursor(s.view_matrices); // 44FD60 in42E6E0 before42F090 resource binding
    s.effect_resources.colors.initialize_allocation_434090(
        s.model_draws, r.scene); // 4342C0 -> full434090 and fresh diagnostic lifetime
    allocate_world_polygon_frame(s.polygon_frame);
    // Preview compatibility publication to the existing screen array. Only
    // 42E4DC is implemented here; this is NOT full42E430 allocation/call order.
    // SourceColdAttachmentOnly above intentionally leaves the base NULL.
    (void)publish_source_screen_base_42e4dc(resources_->platform);
    r.scene.bind_models(bindings.requests, s.selection, s.cars, canonical(std::string(plan.level.texture_name())));
    // Descriptor5C is the same word consumed by existing vehicle presentation
    // and the source model producer. Transfer its imported value once; retain
    // a reference to the canonical world slot rather than a second LOD cache.
    for (unsigned slot = 0; slot < s.cars.size(); ++slot)
        if (const auto model = r.scene.models().resolve(s.cars[slot].source_word00))
            r.scene.models().at(*model).bind_current_detail_level(s.selected_lod[slot]);
    r.pmd = read_file(r.root / "meshdata" / (*bindings.track_name + ".PMD"));
    const auto track_model = r.scene.model_handle(3);
    if (!track_model)
        throw FormatError("track normal binding requires the actual loaded track model");
    bind_track_normal_allocation(r.pmd, r.scene.models().at(*track_model).source_normals(), r.track_normals);
    r.splines = std::make_unique<SplineSet>(r.pmd);
    r.geometry = contact::decode_track_collision(r.pmd);
    r.raw_texture_labels = decode_pmd(r.pmd).textures;
    s.texture_animations.emplace(r.pmd); // 42EF50; no fallback for a missing track section
    if (!s.track_part_animations)
        s.track_part_animations.emplace(); // one allocation of all32 persistent source records
    initialize_track_part_resources(s.scene_mode, &r.scene.models().at(*track_model), r.pmd,
                                    s.track_part_animations->records(),
                                    s.track_part_preparation); // full426260, actual425DF0 before each state copy
    const auto config = canonical("config\\levels\\" + r.level_name + ".cfg");
    const auto available = r.assets.resources(config);
    const auto chosen = std::find(available.begin(), available.end(), config) != available.end()
                            ? config
                            : "config\\levels\\default.cfg";
    update_viewport_ui_scale(s.viewport); // 425070 in scene42E6E0, before402420
    // 42E73A/42E73F:413190 always consumes413110's shared random draws,
    // then changes every matching texture name before42C510 uploads images.
    randomize_vehicle_liveries(s.vehicle_texture_selection, s.random);
    remap_level_vehicle_textures(r.scene.models(), s.cars, s.vehicle_texture_selection.random_liveries);

    std::array<CameraVehicle, 6> observed;
    CameraEffectTargets effects;
    for (unsigned i = 0; i < 6; ++i)
    {
        observed[i] = s.vehicles.camera_snapshot(i);
        effects.vehicles[i] = &s.vehicles.physics[i];
        effects.active[i] = std::bit_cast<std::uint32_t>(s.vehicles.race[i].active);
    }
    camera_initialize_level({s.controls, s.camera_lifecycle, s.camera_presentation, s.camera_bank, s.camera_session,
                             observed, s.selected_camera_preset, camera_vehicle_reset_adapter(effects),
                             s.shared_model_identity},
                            {r.assets, *bindings.track_name, r.pmd, *r.splines}, s.scene_mode, s.split_setting);
    for (unsigned i = 0; i < 6; ++i)
        s.vehicles.store_camera_settings(i, observed[i]);
    seed_world_model_draws(s.model_draws, r.scene.models(), r.scene.model_handle(3), r.scene.model_handle(2),
                           s.sprite_packets, s.level.ground_level, s.scene_mode, s.view_matrices,
                           &r.scene);          // 42E791 ->42B740
    reset_view_matrix_cursor(s.view_matrices); // 42E7AA, after load-time queue seed
    s.lights = decode_pmd_lights(r.pmd);
    initialize_pmd_lights(s.lights, s.ambient_light);
    s.light_vertices.resize(0x4800); // source624530 allocation; retained between42DCF0 calls
    set_light_gamma(s.light_fade, 1, nullptr);
    r.scene.models().at(*track_model).source_outer_transform() =
        &s.model_draws.template_pose;            // 42E7CC, sole mutable4864E8
    s.polygon_frame.low_quality_initialized = 0; // 42E7ED; not a per-frame reset
    // 442030 invokes4469F0 after42E6E0, including its42D700 light producer.
    const auto level_configuration = text_bytes(r.assets.resource(chosen));
    apply_level_lighting(level_configuration, s.ambient_light, s.fog_color);
    apply_level_curves(level_configuration, s.curves);
    auto environment = audio_environment_world(*this);
    load_audio_environments(r.assets.resource(chosen), environment);  // 446700 before4467B0
    s.stunt_knots = decode_ai_stunt_knots(r.assets.resource(chosen)); // 4467B0, sole499FB8
    initialize_cold_numeric_storage(s.vehicles.cold_numeric(s.current_slot), s.gravity.magnitude);
    initialize_cold_presentation(s.vehicles.cold_presentation(), r.scene.model_handle(4), r.scene.model_handle(5),
                                 s.model_draws.template_pose);
    initialize_cold_vehicle_extras(s.vehicles.initialization_extras);
    initialize_world_frame_globals(s.world_frame);
    auto reset_parameters_snapshot = reset_parameters();
    auto reset_vehicles = s.vehicles.reset_views();
    VehicleResetWorld reset{reset_vehicles, s.current_slot, s.controls, s.reset_globals, reset_parameters_snapshot};
    const auto camera = s.controls.current;
    if (!initialize_vehicle_bootstrap(
            {s.vehicles, r.assets, r.scene.models(), reset, *r.splines, s.grid, s.strip, r.scene},
            {s.cars, s.load_selection.configuration_id, std::bit_cast<std::int32_t>(s.control_kind),
             s.controls.controllers[camera].camera.position, s.camera_presentation[camera].forward_axis}))
        throw FormatError("selected track has no starting spline");
    s.current_slot = reset.current;
    initialize_color_tables(s.color_tables);
    reset_replays(s.replays, s.vehicles.race);
    reset_parameters_snapshot = reset_parameters();
    reset.current = s.current_slot;
    const auto selected_ids = selected_vehicles();
    if (launch.startup_observer)
        launch.startup_observer->before_race_initialization(*this);
    const auto initialization_observation =
        initialize_race({s.vehicles,
                         r.assets,
                         reset,
                         *r.splines,
                         s.grid,
                         s.load_selection,
                         s.clock,
                         s.automatic,
                         {s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                         s.selected_camera_preset,
                         {s.frame_projection, s.inverse_depth_scale, s.depth_offset, s.shared_model_identity},
                         s.startup_counters,
                         s.startup,
                         audio,
                         s.music},
                        {s.cars, s.selection, selected_ids, std::bit_cast<std::int32_t>(s.control_kind), s.scene_mode,
                         s.level.music_track});
    s.current_slot = reset.current;
    if (launch.startup_observer)
        launch.startup_observer->after_race_initialization(*this, initialization_observation);
    attach_world_polygon_level(s.polygon_frame); // host lease: all level resource owners now exist
    resources_->readiness = RaceSessionReadiness::preview_ready;
}
RaceSession::~RaceSession() = default; // Resources handles preview failure and teardown
RaceSessionReadiness RaceSession::readiness() const noexcept
{
    return resources_->readiness;
}
void RaceSession::require_preview_resources() const
{
    resources_->platform->require_owner_thread();
    // The preview observer can inspect constructed owners during its original
    // startup callbacks; source attachment has no per-level resources at all.
    if (!resources_->preview)
        throw FormatError("source attachment has no loaded scene resources");
}
const std::shared_ptr<RaceSessionPlatform> &RaceSession::shared_platform() const noexcept
{
    return resources_->platform;
}
const AssetStore &RaceSession::assets() const
{
    return resources_->platform->assets();
}
ModelResources &RaceSession::models()
{
    require_preview_resources();
    return resources_->preview->scene.models();
}
SceneResources &RaceSession::scene_resources()
{
    require_preview_resources();
    return resources_->preview->scene;
}
const SplineSet &RaceSession::splines() const
{
    require_preview_resources();
    if (!resources_->preview->splines)
        throw FormatError("session spline resources have not been loaded");
    return *resources_->preview->splines;
}
const contact::TrackGeometry &RaceSession::track_geometry() const
{
    require_preview_resources();
    return resources_->preview->geometry;
}
WindowsAudioDevice &RaceSession::audio_device()
{
    return resources_->platform->audio_device();
}
WindowsAudioStreamPlatform &RaceSession::audio_stream_platform()
{
    return resources_->platform->audio_stream_platform();
}
std::span<const std::uint8_t> RaceSession::track_pmd() const
{
    require_preview_resources();
    return resources_->preview->pmd;
}
TrackNormalView RaceSession::track_normal_view()
{
    require_preview_resources();
    const auto track = resources_->preview->scene.model_handle(3);
    if (!track)
        throw FormatError("track normal view requires an active track resource");
    return {resources_->preview->scene.models().at(*track).source_normals(), resources_->preview->track_normals};
}
std::span<const std::string> RaceSession::raw_texture_labels() const
{
    require_preview_resources();
    return resources_->preview->raw_texture_labels;
}
const std::string &RaceSession::level_name() const
{
    require_preview_resources();
    return resources_->preview->level_name;
}
std::string_view RaceSession::text(unsigned index) const
{
    require_preview_resources();
    const auto value = state_->game_text.text(index);
    if (!value)
        throw FormatError("required original game text absent");
    return *value;
}
std::uint64_t RaceSession::window() const
{
    require_preview_resources();
    return resources_->preview->window;
}
AudioDriver RaceSession::audio_view()
{
    return {state_->audio_driver, audio_device(), state_->audio_level, state_->controls, state_->split_setting};
}
std::array<std::uint32_t, 4> RaceSession::selected_vehicles() const
{
    require_preview_resources();
    return {state_->trial_selection.split_selection, state_->other_selected_vehicles[0],
            state_->other_selected_vehicles[1], state_->other_selected_vehicles[2]};
}
VehicleResetParameters RaceSession::reset_parameters() const
{
    const auto &s = *state_;
    const auto descriptors = splines().descriptors();
    if (descriptors.empty())
        throw FormatError("track has no reset spline descriptor");
    const auto spline = splines().find(descriptors.front().id);
    if (!spline)
        throw FormatError("first track spline cannot be resolved");
    VehicleResetParameters p;
    p.stage = std::bit_cast<std::int32_t>(s.clock.phase);
    p.mode = std::bit_cast<std::int32_t>(s.clock.game_mode);
    p.selector = std::bit_cast<std::int32_t>(s.level.game_type);
    p.normal_energy_cost = s.gameplay.values[1];
    p.upgraded_energy_cost = s.gameplay.values[3];
    p.course_length = s.clock.course_length;
    // 418D20 reads this adjacent descriptor word only after saved_24a4!=0.
    // CHALL1 ends before it: preserve absence until that source branch needs it.
    const auto spacing_at = 0x148 + std::size_t(word(track_pmd(), 0x78)) + 0x24;
    if (spacing_at <= track_pmd().size() && track_pmd().size() - spacing_at >= 4)
        p.spacing_word = word(track_pmd(), spacing_at);
    p.spline = *spline;
    p.mask = s.curves.respawn;
    p.unit_gravity = s.gravity.unit;
    p.tick = s.simulation_tick;
    for (unsigned i = 0; i < 6; ++i)
        p.vehicle_names[i] = bounded_text(s.cars[i].name);
    p.special_end_text = s.game_text.required(GameTextId::rejoins_the_race);
    p.retired_text = s.game_text.required(GameTextId::is_out_of_the_race);
    return p;
}
} // namespace sgn::retail
