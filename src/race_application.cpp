#include "sgn/race_application.hpp"
#include "sgn/playable_driver.hpp"
#include "sgn/playable_camera.hpp"
#include "sgn/race_application_polygon_backend.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/race_session_pause_restart.hpp"
#include "sgn/session_effect_resources.hpp"
#include "sgn/source_allocation_bank.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/playable_menu_input.hpp"
#include "sgn/race_session_iteration.hpp"
#include "sgn/sourcefront_hud_diagnostics.hpp"
#include "sgn/displayed_recipe_export.hpp"
#include "sgn/front_recipe_bundle.hpp"
#include "sgn/world_join_sidecar.hpp"
#include "sgn/session_world_join.hpp"
#include <bit>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace sgn
{
using namespace retail;
namespace
{
// NEW D3D12 single-stage profile for the already qualified polygon pipeline.
// Source42FA80 supplies its recovered render-state writes. These otherwise
// unspecified texture-stage defaults describe this backend, not a 2001 driver.
PolygonDeviceState checkpoint_device_seed()
{
    PolygonDeviceState state;
    for (unsigned stage = 0; stage < 2; ++stage)
    {
        for (auto [index, value] : std::initializer_list<std::pair<unsigned, unsigned>>{{1, 1},
                                                                                        {2, 2},
                                                                                        {3, 1},
                                                                                        {4, 1},
                                                                                        {5, 2},
                                                                                        {6, 1},
                                                                                        {11, stage},
                                                                                        {13, 1},
                                                                                        {14, 1},
                                                                                        {16, 2},
                                                                                        {17, 2},
                                                                                        {18, 0}})
            state.stage[stage][index] = value;
        state.texture[stage] = 0;
    }
    return state;
}
constexpr PolygonSubmissionCaptureLimits checkpoint_capture_limits{{{20000, 32u * 1024u * 1024u}, 30000}, 16};
std::string timer_text(std::int64_t ticks)
{
    const auto value = static_cast<std::uint64_t>(std::max<std::int64_t>(0, ticks));
    std::ostringstream text;
    text << std::setfill('0') << std::setw(2) << value / 3600 << ':' << std::setw(2) << (value / 60) % 60 << '.'
         << std::setw(2) << (value % 60) * 100 / 60;
    return text.str();
}
std::uint32_t source_running_mode(RaceApplicationMode mode)
{
    return mode == RaceApplicationMode::normal_race ? 2u : mode == RaceApplicationMode::challenge ? 3u : 4u;
}
std::string_view mode_label(RaceApplicationMode mode)
{
    return mode == RaceApplicationMode::normal_race ? "WYSCIG"
           : mode == RaceApplicationMode::challenge ? "WYZWANIE"
                                                    : "PROBA CZASOWA";
}
std::string_view mode_name(RaceApplicationMode mode)
{
    return mode == RaceApplicationMode::normal_race ? "normal_race"
           : mode == RaceApplicationMode::challenge ? "challenge"
                                                    : "time_trial";
}
} // namespace
struct RaceApplication::Impl
{
    std::unique_ptr<RaceSession> session;
    // Declaration order retains every borrowed owner through backend teardown.
    MaterialTransparencyStorage transparency;
    std::unique_ptr<SessionWorldMaterials> materials;
    std::unique_ptr<MaterialD3D12> material_backend;
    std::unique_ptr<PolygonSubmissionState> scratch;
    std::unique_ptr<PolygonDeviceContext> polygon_device;
    std::unique_ptr<RaceApplicationPolygonBackend> polygon;
    std::unique_ptr<SessionPauseRestartSource> restart;
    std::unique_ptr<PlayableDriver> driver;
    PlayableMenuInput menu_input;
    std::uint64_t driver_steps{}, paused_steps{}, camera_begin_calls{}, pause_entries{};
    std::string before_pause{"null"};
    bool exit{};
    std::string diagnostic_snapshot() const;
    diagnostics::HudTrace source_hud_trace;
    using WorldJoinTable = diagnostics::WorldJoinRetention<diagnostics::SessionWorldJoin>;
    std::optional<WorldJoinTable> world_join;
    diagnostics::HudObservation observe_source_hud() const noexcept;
    const std::uint32_t device_e4{1}, device_e8{0}, device_f0{2}, device_depth{1};
    const std::uint16_t texture_units{1}, blend_stages{1};
    std::uint64_t source_captures{}, source_commands{}, physical_draws{};
    bool interpolation_requested{true}, menu{};
    std::string hud_title, hud_detail;
    void capture_hud();
    void capture_source_scene();
    std::uint64_t completed_ticks{}, input_releases{}, throttle_ticks{}, steering_ticks{}, manual_throttle_ticks{},
        manual_steering_ticks{};
    Vec3 initial{}, previous{};
    double travelled{};
    std::array<std::uint64_t, 6> contact_ticks{};
    std::array<Vec3, 6> vehicle_previous{};
    std::array<double, 6> vehicle_travelled{}, racing_travelled{};
    std::array<std::uint64_t, 6> racing_ticks{}, ai_eligible_ticks{};
    std::vector<std::pair<std::uint64_t, Vec3>> samples;
    std::vector<std::pair<std::uint64_t, unsigned>> input_events;
    unsigned input_mask{256};
    bool confirm_down{};
    PlayableInput last_input{};
    std::string level;
    RaceApplicationMode mode;
    unsigned primary() const
    {
        const auto &controls = session->state().controls.controllers;
        for (const auto &c : controls)
            if (c.active == 1 && c.vehicle < 6)
                return c.vehicle;
        throw FormatError("race has no active player controller");
    }
    Impl(const std::filesystem::path &root, const RaceApplicationOptions &options, HWND window, Renderer &renderer,
         SourceApplicationLiteralData *literals)
        : level(options.level), mode(options.mode)
    {
        if (options.world_join)
            world_join.emplace(*options.world_join);
        RaceSessionLaunch launch;
        launch.level_name = level;
        launch.width = static_cast<std::int32_t>(renderer.stats().width);
        launch.height = static_cast<std::int32_t>(renderer.stats().height);
        launch.mode = mode == RaceApplicationMode::normal_race ? SessionMode::normal_race
                      : mode == RaceApplicationMode::challenge ? SessionMode::challenge
                                                               : SessionMode::time_trial;
        launch.challenge_ticks = options.challenge_ticks;
        interpolation_requested = options.interpolation_enabled;
        launch.window = reinterpret_cast<std::uint64_t>(window);
        launch.master_gain = options.muted ? 0.0f : 1.0f;
        launch.music_percent = 100; // Recovered music path; host --muted changes only the final mixer gain.
        if (literals)
            session = std::make_unique<RaceSession>(PreviewHostEntry{}, root, launch, *literals);
        else
            session = std::make_unique<RaceSession>(root, launch);
        menu_input.bind_keyboard(*session);
        // Explicit preview composition. This is not activation of cold442120.
        // The same retained source bank now owns the qualified font scratch.
        auto &source_bank = session->shared_platform()->source_allocations();
        source_bank.initialize_42eab0();
        materials = std::make_unique<SessionWorldMaterials>(*session);
        material_backend = std::make_unique<MaterialD3D12>(renderer, transparency, false, 1);
        material_backend->reset_transparency_430690();
        const auto roots = source_graphics_roots_from_config(session->state().boot_config);
        WorldMaterialLoadingContext loading;
        loading.source_graphics_roots = &roots;
        loading.scene = session->state().scene_mode;
        const auto &prefix = session->state().level.track_texture;
        const auto end = std::find(prefix.begin(), prefix.end(), '\0');
        if (end == prefix.end())
            throw FormatError("checkpoint track texture prefix is unterminated");
        loading.track_prefix = {prefix.begin(), end};
        // NEW host upload policy preserves the actual PC texture dimensions.
        loading.quality = 0;
        loading.maximum_dimension = 4096;
        materials->load_with_source_fonts(loading, *material_backend);
        materials->publish_count_42e772();
        scratch = std::make_unique<PolygonSubmissionState>();
        prepare_polygon_submission_ui_capacity(*scratch, desktop_polygon_submission_budget);
        polygon_device = std::make_unique<PolygonDeviceContext>(checkpoint_device_seed());
        auto &state = session->state();
        reset_device_source_globals_42f300(state.outer.device, *scratch);
        DeviceSetupDevice *device = polygon_device.get();
        DeviceSetupSurface *surface = nullptr;
        if (initialize_retail_device({{device_e4, device_e8, device_f0, texture_units, blend_stages, device},
                                      session_outer_device_setup_globals(state),
                                      device,
                                      surface}) != 1)
            throw FormatError("checkpoint source device setup failed");
        polygon = std::make_unique<RaceApplicationPolygonBackend>(*session, *materials, *scratch, *polygon_device,
                                                                  *material_backend, renderer, session->assets());
        restart = std::make_unique<SessionPauseRestartSource>(*session);
        driver = std::make_unique<PlayableDriver>(*session, polygon->textures(),
                                                  borrow_session_effect_animation_frames(*session));
        const auto &p = session->state().vehicles.physics[primary()].position;
        initial = previous = {p.x, p.y, p.z};
        samples.emplace_back(0, initial);
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto &position = session->state().vehicles.physics[i].position;
            vehicle_previous[i] = {position.x, position.y, position.z};
        }
    }
};
RaceApplication::RaceApplication(const std::filesystem::path &root, const RaceApplicationOptions &options, HWND window,
                                 Renderer &renderer)
    : impl_(std::make_unique<Impl>(root, options, window, renderer, nullptr))
{
}
RaceApplication::RaceApplication(const std::filesystem::path &root, const RaceApplicationOptions &options, HWND window,
                                 Renderer &renderer, retail::SourceApplicationLiteralData &literals)
    : impl_(std::make_unique<Impl>(root, options, window, renderer, &literals))
{
}
RaceApplication::~RaceApplication() = default;
void RaceApplication::release_input()
{
    impl_->menu_input.release_host_input();
    // Paused source histories survive focus loss. The next accepted menu
    // publication receives the host's cleared keys; resume maps vehicle keys.
    if (impl_->session->state().frame_pause == 0)
        impl_->driver->release_input();
    impl_->last_input = {};
    impl_->confirm_down = false;
    impl_->session->state().normal_finish.confirm_pressed = 0;
    ++impl_->input_releases;
}
void RaceApplication::tick(const std::array<bool, 256> &keys, bool reset, bool scripted, Renderer &renderer)
{
    auto &w = *impl_;
    PlayableInput input;
    if (menu_requested() || exit_requested())
        return;
    w.polygon->prepare_source_tick(); // May recover publication; no input/source/audio has run yet.
    const auto entry_pause = w.session->state().frame_pause;
    // Snapshot precedes the actual Escape callback, so pause invariants can
    // compare the last driving state against the final paused source state.
    std::string entry_diagnostic;
    if (entry_pause == 0 && keys[VK_ESCAPE])
        entry_diagnostic = w.diagnostic_snapshot();
    w.menu_input.publish(*w.session, keys);
    const bool paused = w.session->state().frame_pause == 1;
    if (entry_pause == 0 && paused)
    {
        w.before_pause = std::move(entry_diagnostic);
        ++w.pause_entries;
    }
    if (!paused)
    {
        // New keyboard binding to the two source confirmation owners. Held Enter
        // does not repeatedly clear443D80's consumed latch.
        const bool confirm = !scripted && keys[VK_RETURN];
        w.session->state().normal_finish.confirm_pressed = confirm ? 1u : 0u;
        if (confirm && !w.confirm_down)
            w.session->state().normal_finish.confirm_consumed = 0;
        w.confirm_down = confirm;
        if (scripted)
        {
            const auto t = w.completed_ticks;
            input.throttle = t < 1140;
            input.brake = t >= 1140;
            input.left = t >= 900 && t < 930;
            input.right = t >= 960 && t < 990;
            input.turbo = t >= 840 && t < 900;
        }
        else
        {
            input.throttle = keys['W'] || keys[VK_UP];
            input.brake = keys['S'] || keys[VK_DOWN];
            input.left = keys['A'] || keys[VK_LEFT];
            input.right = keys['D'] || keys[VK_RIGHT];
            input.turbo = keys[VK_SPACE];
            input.stunt = keys[VK_SHIFT];
            input.shift = keys['E'];
            input.reset = reset;
        }
        const unsigned mask = unsigned(input.throttle) | (unsigned(input.brake) << 1) | (unsigned(input.left) << 2) |
                              (unsigned(input.right) << 3) | (unsigned(input.turbo) << 4) |
                              (unsigned(input.stunt) << 5) | (unsigned(input.shift) << 6) |
                              (unsigned(input.reset) << 7);
        if (mask != w.input_mask && w.input_events.size() < 50000)
        {
            w.input_events.emplace_back(w.completed_ticks, mask);
            w.input_mask = mask;
        }
        playable_camera_begin_tick(*w.session);
        ++w.camera_begin_calls;
        w.driver->tick(input);
        ++w.driver_steps;
    }
    else
    {
        // 442838..44287C skips the world at4423D0 but retains the suffix.
        //  No vehicle publication/history or camera outer preparation here.
        advance_session_iteration(*w.session, w.polygon->textures(),
                                  borrow_session_effect_animation_frames(*w.session));
        ++w.paused_steps;
    }
    // New host scheduling policy: one source outer-frame audio maintenance
    // per 60Hz simulation tick, after 441F70 and before normal-view staging.
    maintain_session_audio(*w.session);
    (void)renderer; // GPU/source capture uses the same backend bound at construction.
    w.capture_source_scene();
    const auto &transition = w.session->state();
    // NEW preview lifetime boundary after the real446B20 ->441E20 request.
    // Preserve pending/target; the unintegrated442030 frontend is not claimed.
    w.exit = transition.normal_finish.menu_requested == 1 && transition.options.selection_phase == 2 &&
             transition.options.transient == 1;
    ++w.completed_ticks;
    w.last_input = input;
    w.throttle_ticks += input.throttle;
    w.steering_ticks += input.left || input.right;
    const auto &state = w.session->state();
    const auto &p = state.vehicles.physics[w.primary()].position;
    const auto &control = state.vehicles.race[w.primary()];
    const auto running_mode = source_running_mode(w.mode);
    if (!paused && state.clock.game_mode == running_mode && state.clock.phase == 2 && control.control_enabled == 1 &&
        control.automatic_drive == 0)
    {
        w.manual_throttle_ticks += input.throttle;
        w.manual_steering_ticks += input.left || input.right;
    }
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw FormatError("recovered player position is nonfinite");
    const double dx = double(p.x) - w.previous.x, dy = double(p.y) - w.previous.y, dz = double(p.z) - w.previous.z;
    w.travelled += std::sqrt(dx * dx + dy * dy + dz * dz);
    w.previous = {p.x, p.y, p.z};
    if (w.completed_ticks % 30 == 0 && w.samples.size() < 20001)
        w.samples.emplace_back(w.completed_ticks, w.previous);
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto &v = state.vehicles.physics[i];
        const auto &r = state.vehicles.race[i];
        if (r.active == 1)
        {
            for (float value : {v.position.x, v.position.y, v.position.z})
                if (!std::isfinite(value))
                    throw FormatError("active recovered vehicle position is nonfinite");
            w.contact_ticks[i] += !paused && v.contact_class != 0;
            const double x = double(v.position.x) - w.vehicle_previous[i].x,
                         y = double(v.position.y) - w.vehicle_previous[i].y,
                         z = double(v.position.z) - w.vehicle_previous[i].z;
            const double distance = std::sqrt(x * x + y * y + z * z);
            w.vehicle_travelled[i] += distance;
            if (!paused && state.clock.game_mode == running_mode && state.clock.phase == 2 && r.automatic_drive == 0)
            {
                ++w.racing_ticks[i];
                w.racing_travelled[i] += distance;
                // Eligibility observation only; original AI execution is
                // separately checked by the driver's source-call probes.
                w.ai_eligible_ticks[i] += v.upgrade_active != 0;
            }
        }
        w.vehicle_previous[i] = {v.position.x, v.position.y, v.position.z};
    }
    w.capture_hud();
}
diagnostics::HudObservation RaceApplication::Impl::observe_source_hud() const noexcept
{
    const auto &s = session->state();
    return diagnostics::observe_hud(s.simulation_tick, s.frame_pause, s.clock.game_mode, s.split_setting, s.controls,
                                    s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins, s.vehicles.physics,
                                    s.vehicles.race, s.viewport);
}
void RaceApplication::Impl::capture_source_scene()
{
    SessionWorldFrameResources resources;
    publish_session_effect_resources(*session, resources);
    resources.gamma = &polygon->gamma_device();
    resources.submission_budget = desktop_polygon_submission_budget;
    const auto accepted_serial = completed_ticks + 1;
    const auto source_tick = session->state().simulation_tick;
    std::optional<WorldJoinTable::Token> join_token;
    if (world_join)
        join_token = world_join->reserve(accepted_serial, source_tick, polygon->displayed(),
                                         [] { return std::make_unique<diagnostics::SessionWorldJoin>(true); });
    auto *join = world_join && join_token ? world_join->pending(*join_token) : nullptr;
    if (join && join->prepare(*session, *polygon_device, accepted_serial, source_tick))
        resources.geometry_audit.world_join = join;
    struct AbortPendingJoin
    {
        WorldJoinTable *table;
        std::optional<WorldJoinTable::Token> token;
        ~AbortPendingJoin() noexcept
        {
            if (table)
                table->cancel(token);
        }
    } cleanup{world_join ? &*world_join : nullptr, join_token};
    const SessionOuterWorldResourceBorrow world{*session, resources, SessionOuterGammaBinding::source_device_present};
    const SessionOuterBoundCaptureRequest request{*restart,    SelectedDeviceDepthBorrow(device_depth),
                                                  world,       accepted_serial,
                                                  source_tick, checkpoint_capture_limits};
    // Source draw children once per accepted 60Hz step. No second input,
    // iteration, audio-maintenance call, camera path or physical Present.
    const auto hud_before = observe_source_hud();
    const auto frame = polygon->capture_source_frame_deferred(request);
    if (frame.submission)
    {
        ++source_captures;
        source_commands += frame.submission->commands;
        source_hud_trace.record(frame.accepted_outer_serial, frame.submission->source_tick, hud_before,
                                observe_source_hud());
    }
    if (world_join)
        world_join->complete(join_token, frame.accepted_outer_serial, frame.submission);
}
void RaceApplication::suspend_renderer_for_recreation()
{
    impl_->polygon->suspend_renderer_for_recreation();
}
void RaceApplication::bind_renderer(Renderer &renderer)
{
    impl_->polygon->bind_renderer(renderer);
}
void RaceApplication::resize(unsigned width, unsigned height)
{
    if (width == 0 || height == 0)
        throw FormatError("race drawable extent is empty");
    playable_camera_resize(*impl_->session, width, height);
    // Existing backend restores immutable front/back recipes on next draw.
    // Resize never dispatches a source frame or advances the camera/physics.
}
void RaceApplication::Impl::capture_hud()
{
    auto &w = *this;
    const auto &s = w.session->state();
    const auto &car = s.vehicles.race[w.primary()];
    menu = s.normal_finish.menu_requested != 0 && !exit;
    hud_title = w.level + " / " + std::string(mode_label(w.mode));
    std::string detail;
    if (w.mode == RaceApplicationMode::normal_race && s.vehicles.physics[w.primary()].energy_exempt == 1)
    {
        // 41DA70 publishes 24E0 and copies the final zero-based place to24E4.
        // Keep showing those source results while its finish presentation runs.
        detail = "WYSCIG UKONCZONY    MIEJSCE " + std::to_string(car.finish_state + 1) + "/" +
                 std::to_string(s.ranking.count) + "    CZAS " + timer_text(car.race_timer);
        detail += menu ? "    ENTER - NOWY WYSCIG" : "    ENTER - DALEJ";
    }
    else if (s.clock.phase == 1 || s.clock.game_mode == 1 || car.automatic_drive != 0)
        detail = "TRWA START";
    else if (w.mode == RaceApplicationMode::normal_race)
    {
        // New text layout over actual counters.41D8F0 uses race_index
        // (23A4) for LAP n/total; the display cap is a frontend policy.
        if (s.ranking.count == 0 || s.ranking.count > 6 || car.place >= s.ranking.count)
            throw FormatError("normal race HUD ranking is not published");
        const auto lap = std::min(static_cast<unsigned>(std::max(0, car.race_index)), s.clock.lap_count);
        detail = "POZYCJA " + std::to_string(car.place + 1) + "/" + std::to_string(s.ranking.count) + "    OKRAZENIE " +
                 std::to_string(lap) + "/" + std::to_string(s.clock.lap_count) + "    CZAS " +
                 timer_text(car.race_timer);
    }
    else if (w.mode == RaceApplicationMode::challenge)
        detail = "POZOSTALO " + timer_text(s.clock.remaining_ticks) + "    PUNKTY " + std::to_string(car.stunt_score);
    else
        detail = "OKRAZENIA " + std::to_string(std::max(0, car.lap_index)) + "    CZAS " + timer_text(car.lap_timer);
    if (menu && s.vehicles.physics[w.primary()].energy_exempt != 1)
        detail = "KONIEC SESJI    ENTER - NOWY WYSCIG";
    if (menu && w.mode == RaceApplicationMode::challenge)
        detail = "KONIEC WYZWANIA    PUNKTY " + std::to_string(car.stunt_score) + "    ENTER - NOWE WYZWANIE";
    hud_detail = std::move(detail);
}
void RaceApplication::invalidate_presentation() noexcept
{
    // Front/back recipes are immutable. Focus suspension retains both images.
}
bool RaceApplication::ready_to_draw() const noexcept
{
    return bool(impl_->polygon->displayed());
}
void RaceApplication::draw(Renderer &renderer, float dpi, bool active, double alpha)
{
    (void)dpi;
    (void)active;
    (void)alpha;
    // Every extra refresh presents the selected source front only. The native
    // capture hook records that actual presented surface before Present.
    if (impl_->polygon->draw(renderer))
        ++impl_->physical_draws;
}
std::uint64_t RaceApplication::ticks() const noexcept
{
    return impl_->completed_ticks;
}
bool RaceApplication::menu_requested() const noexcept
{
    return impl_->menu;
}
bool RaceApplication::exit_requested() const noexcept
{
    return impl_->exit;
}
std::string RaceApplication::Impl::diagnostic_snapshot() const
{
    const auto &s = session->state();
    std::ostringstream out;
    out << "{\"host_tick\":" << completed_ticks << ",\"source_tick\":" << s.simulation_tick
        << ",\"frame_pause\":" << s.frame_pause << ",\"driver_steps\":" << driver_steps
        << ",\"camera_begin_calls\":" << camera_begin_calls << ",\"vehicles\":[";
    for (unsigned slot = 0; slot < 6; ++slot)
    {
        if (slot)
            out << ',';
        const auto &p = s.vehicles.physics[slot];
        const auto &current = p.input;
        const auto &prior = s.vehicles.restart[slot].previous_input;
        out << "{\"position_bits\":[" << std::bit_cast<std::uint32_t>(p.position.x) << ','
            << std::bit_cast<std::uint32_t>(p.position.y) << ',' << std::bit_cast<std::uint32_t>(p.position.z)
            << "],\"input\":[";
        bool first = true;
        for (auto byte : {current.throttle, current.brake, current.left, current.right, current.shift, current.stunt,
                          current.rear_lock, current.steering_boost, current.turbo, current.auxiliary})
        {
            if (!first)
                out << ',';
            first = false;
            out << unsigned(byte);
        }
        out << "],\"previous_input\":[";
        first = true;
        for (auto byte : {prior.throttle, prior.brake, prior.left, prior.right, p.previous_shift, p.previous_stunt,
                          prior.rear_lock, prior.steering_boost, prior.turbo, prior.auxiliary})
        {
            if (!first)
                out << ',';
            first = false;
            out << unsigned(byte);
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}
void RaceApplication::export_displayed_front_bundle(const std::filesystem::path &path) const
{
    const auto captured = impl_->polygon->capture_displayed_recipe();
    if (!captured)
        throw std::runtime_error("no actual front for diagnostic bundle");
    const auto view =
        impl_->world_join
            ? std::optional{diagnostics::world_join_sidecar_view(*impl_->world_join, captured->submission)}
            : std::nullopt;
    diagnostics::export_front_recipe_bundle(path, *captured, view ? &*view : nullptr);
}
#if defined(SGN_CAMERA_RECOVERY_RECEIVING) && SGN_CAMERA_RECOVERY_RECEIVING == 1
diagnostics::CameraRecoverySnapshot RaceApplication::capture_camera_recovery_witness(
    std::uint64_t host_clock_tick, const std::filesystem::path &front_bundle) const
{
    const auto front = impl_->polygon->capture_displayed_recipe();
    if (!front)
        throw std::runtime_error("camera/front recovery witness requires an actual selected front");
    const auto observation =
        diagnostics::capture_camera_recovery_state(impl_->session->state(), impl_->session->readiness(),
                                                   {host_clock_tick, impl_->completed_ticks, impl_->driver_steps,
                                                    impl_->camera_begin_calls, impl_->source_captures},
                                                   front);
    // The SAME const snapshot supplies both witness gamma and this exact bundle.
    // No world-retention selection is demanded for a pre-fault front endpoint.
    diagnostics::export_front_recipe_bundle(front_bundle, *front);
    return observation; // Owned bytes only; the retained frame copy dies here.
}
#endif
void RaceApplication::export_displayed_recipe(const std::filesystem::path &path) const
{
    diagnostics::export_displayed_recipe(path, impl_->polygon->displayed());
}
bool RaceApplication::export_displayed_world_join(const std::filesystem::path &recipe,
                                                  const std::filesystem::path &sidecar) const
{
    if (!impl_->world_join)
        throw std::invalid_argument("world join diagnostics were not selected");
    const auto &displayed = impl_->polygon->displayed();
    const auto view = diagnostics::world_join_sidecar_view(*impl_->world_join, displayed);
    return diagnostics::export_displayed_world_join(recipe, sidecar, displayed, view);
}
void RaceApplication::report(std::ostream &out) const
{
    const auto &w = *impl_;
    const auto &s = w.session->state();
    const auto player = w.primary();
    const auto &stream = s.audio_stream;
    const auto &music = s.audio_maintenance.music;
    AudioVoiceInfo voice{};
    bool voice_inspected = false;
    if (music.buffer.value)
    {
        try
        {
            voice = w.session->audio_device().inspect(music.buffer.borrowed());
            voice_inspected = true;
        }
        catch (const std::exception &)
        { /* Preserve the primary runtime failure report. */
        }
    }
    const auto vector = [&](Vec3 p) { out << '[' << p.x << ',' << p.y << ',' << p.z << ']'; };
    out << "{\"fixed_hz\":60,\"ticks\":" << w.completed_ticks << ",\"source_tick\":" << s.simulation_tick
        << ",\"player_slot\":" << player << ",\"session_mode\":\"" << mode_name(w.mode)
        << "\",\"game_type\":" << s.level.game_type << ",\"render_quality\":" << s.impact_particles.rate_selector
        << ",\"menu_requested\":" << s.normal_finish.menu_requested << ",\"mode\":" << s.clock.game_mode
        << ",\"phase\":" << s.clock.phase << ",\"countdown_pending\":" << s.clock.countdown_pending
        << ",\"travelled_source_units\":" << w.travelled << ",\"input_releases\":" << w.input_releases
        << ",\"throttle_ticks\":" << w.throttle_ticks << ",\"steering_ticks\":" << w.steering_ticks
        << ",\"manual_throttle_ticks\":" << w.manual_throttle_ticks
        << ",\"manual_steering_ticks\":" << w.manual_steering_ticks
        << ",\"music_connected\":" << (voice_inspected && voice.streaming) << ",\"initial_position\":";
    vector(w.initial);
    out << ",\"final_position\":";
    vector(w.previous);
    out << ",\"pause_diagnostics\":{\"entries\":" << w.pause_entries << ",\"paused_steps\":" << w.paused_steps
        << ",\"before_pause\":" << w.before_pause << ",\"final\":" << w.diagnostic_snapshot() << '}';
    out << ",\"source_hud_diagnostics\":";
    const auto &displayed = w.polygon->displayed();
    diagnostics::write_hud_trace(out, w.source_hud_trace,
                                 displayed ? std::optional<std::uint64_t>(displayed->source_tick) : std::nullopt);
    out << ",\"exit_requested\":" << w.exit << ",\"exit_reason\":\"" << (w.exit ? "pause_menu_exit_host_preview" : "")
        << "\"";
    out << ",\"last_input\":[" << w.last_input.throttle << ',' << w.last_input.brake << ',' << w.last_input.left << ','
        << w.last_input.right << ',' << w.last_input.turbo << "]";
    out << ",\"vehicles\":[";
    for (unsigned i = 0; i < 6; ++i)
    {
        if (i)
            out << ',';
        const auto &v = s.vehicles.physics[i];
        const auto &r = s.vehicles.race[i];
        out << "{\"active\":" << r.active << ",\"position\":";
        vector({v.position.x, v.position.y, v.position.z});
        out << ",\"contact_class\":" << v.contact_class << ",\"contact_ticks\":" << w.contact_ticks[i]
            << ",\"lap\":" << r.lap_index << ",\"race_index\":" << r.race_index << ",\"place\":" << r.place
            << ",\"finish_state\":" << r.finish_state << ",\"finished\":" << (v.energy_exempt == 1)
            << ",\"race_timer\":" << r.race_timer << ",\"progress\":" << r.progress
            << ",\"control_enabled\":" << r.control_enabled << ",\"automatic_drive\":" << r.automatic_drive
            << ",\"upgraded_ai\":" << v.upgrade_active << ",\"travelled\":" << w.vehicle_travelled[i]
            << ",\"racing_ticks\":" << w.racing_ticks[i] << ",\"racing_travelled\":" << w.racing_travelled[i]
            << ",\"ai_driver_eligible_ticks\":" << w.ai_eligible_ticks[i] << '}';
    }
    out << "],\"trajectory\":[";
    for (std::size_t i = 0; i < w.samples.size(); ++i)
    {
        if (i)
            out << ',';
        out << '[' << w.samples[i].first << ',';
        vector(w.samples[i].second);
        out << ']';
    }
    out << "],\"input_event_bits\":\"throttle,brake,left,right,turbo,stunt,shift,reset\",\"input_events\":[";
    for (std::size_t i = 0; i < w.input_events.size(); ++i)
    {
        if (i)
            out << ',';
        out << '[' << w.input_events[i].first << ',' << w.input_events[i].second << ']';
    }
    out << "],\"input_events_truncated\":" << (w.input_events.size() >= 50000)
        << ",\"presentation\":{\"path\":\"source_scene_hud_menu\",\"interpolation_enabled\":false"
        << ",\"interpolation_requested\":" << w.interpolation_requested << ",\"source_captures\":" << w.source_captures
        << ",\"source_commands\":" << w.source_commands << ",\"physical_draws\":" << w.physical_draws
        << ",\"ready_front\":" << ready_to_draw() << ",\"full_cold_activated\":false}"
        << ",\"music\":{\"voice_inspected\":" << voice_inspected << ",\"buffer\":" << music.buffer.value
        << ",\"allocated\":" << stream.allocated << ",\"open_result\":" << stream.result
        << ",\"playback_active\":" << stream.playback_active << ",\"source_track\":" << stream.field_e10c
        << ",\"gain\":" << music.gain << ",\"gain_step\":" << music.gain_step
        << ",\"cached_volume\":" << music.cached_volume << ",\"samples_played\":" << voice.samples_played
        << ",\"queued_buffers\":" << voice.queued << ",\"submitted_bytes\":" << voice.stream_submitted_bytes
        << ",\"completed_buffers\":" << voice.stream_buffers_completed
        << ",\"ring_publications\":" << voice.stream_publications
        << ",\"live_files\":" << w.session->audio_stream_platform().live_files()
        << ",\"live_timers\":" << w.session->audio_stream_platform().live_timers()
        << ",\"audible_quality_verified\":false},\"retail_full_parity_proven\":false}";
}
} // namespace sgn
