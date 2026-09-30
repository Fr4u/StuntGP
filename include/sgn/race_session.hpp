#pragma once
#include "sgn/source_transport_storage.hpp"
#include "sgn/source_transport_matrix.hpp"
#include "sgn/audio_environment.hpp"
#include "sgn/contact_cast.hpp"
#include "sgn/contact_ai_stunts.hpp"
#include "sgn/contact_engine_particles.hpp"
#include "sgn/contact_rail_state.hpp"
#include "sgn/platform_audio.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/retail_gameplay_configuration.hpp"
#include "sgn/retail_game_text.hpp"
#include "sgn/retail_frontend_launch.hpp"
#include "sgn/retail_color_tables.hpp"
#include "sgn/retail_level_curves.hpp"
#include "sgn/retail_light_effects.hpp"
#include "sgn/retail_race_initialization.hpp"
#include "sgn/retail_normal_race_lifecycle.hpp"
#include "sgn/retail_screen_polygons.hpp"
#include "sgn/source_screen_base.hpp"
#include "sgn/source_light_storage.hpp"
#include "sgn/retail_tick_clock.hpp"
#include "sgn/source_scheduler_state.hpp"
#include "sgn/source_cold_time.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/retail_track_normal_allocation.hpp"
#include "sgn/retail_track_part_preparation.hpp"
#include "sgn/retail_trail_render.hpp"
#include "sgn/retail_viewport.hpp"
#include "sgn/retail_world_view_prefix.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/session_ui_state.hpp"
#include "sgn/session_pause_state.hpp"
#include "sgn/session_menu_state.hpp"
#include "sgn/session_menu_input_state.hpp"
#include "sgn/session_outer_source_state.hpp"
#include "sgn/source_input_dispatch.hpp"
#include "sgn/session_effect_resources.hpp"
#include "sgn/track_part_animation.hpp"
#include "sgn/world_model_draw.hpp"
#include "sgn/vehicle_audio.hpp"
#include "sgn/vehicle_dynamics_tick.hpp"
#include "sgn/vehicle_prestart_progression.hpp"
#include <memory>

namespace sgn::retail
{
// Only level words without an existing world owner. Live time/lap values are
// in RaceClockState, records in ProgressionRecordState/VehicleRaceFinishState.
struct RaceSessionLevel
{
    std::uint32_t game_type{}, music_track{}, texture_suffix{}, rolling_start_offset{};
    std::uint32_t camera_mode{}, overlay_demo{}, demo_loop_ticks{};
    float ground_level{};
    std::array<char, 64> track_texture{};
};

// Persistent source owners. Adapter parameters, camera observations and reset
// contexts are built for one invocation and are never retained as extra state.
struct RaceSessionState
{
    VehicleStorage vehicles;
    contact::player::Controls controls;
    VehicleRandom random;
    SourceColdTimeState source_cold_time; // sole CRT time caches; SAME platform world lifetime
    SourceBootConfigState boot_config;    // sole missing boot CFG and file-cache source owners
    AudioEnvironmentState audio_environment{initial_audio_environment_state()};
    WorldGravity gravity;
    WorldFrameGlobals world_frame;
    RaceClockState clock;
    GameplayRules gameplay;
    ScoreRules score_rules;
    // PE descriptor storage only. Reached445D10 is owned by the language loader.
    GameTextState game_text{GameTextState::PeImageOnly{}};
    LevelCurveTables curves;
    ColorTables color_tables;
    RaceSessionLevel level;
    unsigned current_slot{}, selected_camera_preset{}, selected_hud{};
    std::uint32_t split_setting{}, scene_mode{}, control_kind{}, pause_flag{};
    std::uint32_t frame_pause{};      // sole61294C, reset by445FA0
    SessionPauseState pause;          // sole612954/61295C/61B744/60D620/612970; shared platform lifetime
    SessionMenuState menu;            // five menu scalars, eight mutable rows, live label identities
    SessionMenuInputState menu_input; // 612950, physical+0/+4 and eight menu edge bytes
    SourceInputStorage source_input;  // sole raw input/profile owners; retained by shared platform
    SessionOuterSourceState outer;    // missing outer/device/demo words; shared platform world lifetime
    std::uint32_t simulation_tick{}, render_b{}, demo_ticks{}, hud_block{}; // 612CFC/960/990/99C
    RetailTickClock tick_clock;            // sole612D00/04/08; platform clock initialization remains separate
    SourceSchedulerState source_scheduler; // sole60F2AC/60DF9C/612D0C; retained platform owner
    std::uint32_t score_block{};           // 60D680
    std::array<SelectionRecord, 6> selection;
    std::array<LevelCarSlot, 6> cars;
    VehicleTextureSelection vehicle_texture_selection;      // 413110 before413190/413220
    TimeTrialSelectionState trial_selection;                // sole owner of60D754
    std::array<std::uint32_t, 3> other_selected_vehicles{}; // 60D758/75C/760 only
    FrontendLaunchState frontend_launch;                    // sole60D744/748/60DF6C/90 and62D598/59C
    TimeTrialGhosts ghosts;
    TimeTrialStartState trial_start;
    ProgressionRecordState records;
    VehicleRaceFinishState finish;
    NormalRaceFinishState normal_finish;
    RaceRanking ranking;
    ReplayBank replays;
    ReplayWorkspace replay_workspace;
    VehicleGridState grid;
    VehicleLoadSelection load_selection;
    VehicleResetGlobals reset_globals;
    RaceStartupCounters startup_counters;
    RaceInitializationState startup;
    AiAutomaticGlobals automatic;
    AiStuntKnots stunt_knots{}; // sole499FB8, authored4467B0 level configuration
    VehicleScoreState score_popups;
    VehicleDynamicsWorkspace dynamics_workspace;
    SourceTransportMatrix model_basis{}; // sole4D5B30; checked opaque byte transport
    TrailProfileBank trail_profiles{initial_trail_profile_bank()};
    SourceTransportStorage<SourceTrailPolygon> trail_staging{}; // sole571330, source PE zero initialization
    VehicleStripGeometry strip;
    std::array<unsigned, 6> selected_lod{};
    // Host presentation diagnostics; advances only after an actual source placement.
    std::array<std::uint64_t, 6> presentation_vehicle_continuity{};
    RailState rail;
    contact::solver::Workspace contact_workspace;
    contact::ContactCastState contact_cast;
    contact::effects::ImpactParticles impact_particles;
    contact::effects::ImpactParticleDrawList impact_draw_list;
    contact::effects::EffectParticleBank effect_particles;
    contact::effects::EffectStaging effect_staging;
    VehicleAudioGlobals vehicle_audio;
    VehicleAudioFrontier audio_frontier;
    AudioDriverState audio_driver;
    AudioInitializationState audio_initialization;
    AudioMaintenanceState audio_maintenance;
    AudioStreamState audio_stream;
    LevelAudioState audio_level;
    LevelOverrideOptions options;
    AudioMusicTrack music;
    std::array<ControllerOverlayTransition, 4> overlays;
    std::array<ControllerHudCountdown, 4> hud_countdown;
    std::array<RaceHudLayout, 4> hud_layout;
    RaceHudMargins hud_margins;
    SessionUiState ui; // persistent source UI/font/star owners, shared with frontend views
    std::array<CameraLifecycle, 4> camera_lifecycle;
    std::array<CameraPresentationState, 4> camera_presentation;
    CameraBank camera_bank;
    CameraSessionState camera_session;
    CameraMatrix frame_projection{}, frame_world_view{};
    WorldPolygonFrame polygon_frame;      // sole42E430 allocations and retained61C508
    CameraMatrix shared_model_identity{}; // sole61C468; republished by44F4C0
    float inverse_depth_scale{}, depth_offset{};
    ViewportState viewport;
    WorldViewQuality view_quality;
    ViewMatrixArena view_matrices;
    std::optional<TextureAnimationBank> texture_animations;      // actual track42EF50 section
    std::optional<TrackPartAnimationBank> track_part_animations; // sole576CD0, actual426260 state publications
    SourcePmdLocation
        source_track_normal_cursor; // sole source576CC8; retained platform lifetime,426EDC/426607 stores only
    TrackPartPreparationScratch track_part_preparation; // sole5775D0..57764F, retained across resource replacement
    TextureAnimationState texture_animation;
    PmdLightBank lights;
    RetailAmbientLight ambient_light;
    std::array<std::uint32_t, 3> fog_color{}; // sole5896B8/BC/C0, authored446840 override
    LightViewSelection light_selection;
    CameraMatrix source_light_rotation_61c3a4{}; // sole source44EB70 rotation scratch, PE zero
    LightFadeState light_fade;
    LightViewAnimation light_animation;
    GlareState glare{initial_glare_state()};
    SourceSpritePacketStorage sprite_packets;
    SessionEffectResourceStorage effect_resources{sprite_packets}; // same persistent allocation lifetime
    WorldModelDrawState model_draws;
    ScreenPolygonBasePublication screen_base; // sole nullable published61C5F0; payload remains below
    ScreenPolygonStorage screen_polygons;     // 61C5F0 allocation,42E430 capacity
    std::uint32_t screen_polygon_count{};     // sole61C604; reset belongs to425870
    SourceLightStorage light_vertices;
    std::uint32_t light_vertex_offset{};
    // Sole61B634: PE zero-fill,430D93 publication and44600F reset.
    // Survives resource/capture destruction with the existing platform world.
    std::uint32_t source_overlay_selection_61b634{};
};

// Explicit new frontend choices. These are inputs to recovered selection and
// startup routines, not simulated car poses or a precomputed fixture seed.
struct StartupObserver;
enum class SessionMode
{
    time_trial,
    normal_race,
    challenge
};
struct RaceSessionLaunch
{
    SessionMode mode{SessionMode::time_trial};
    std::string level_name{"TRACK1"};
    std::string player_name{"PLAYER"};
    std::uint32_t model{1}, configuration{1}, tyre{1}, livery{2};
    std::uint32_t random_seed{1};
    std::uint32_t challenge_ticks{};                       // 60D768;445120 uses5400 for unsigned values below1200
    std::uint32_t control_kind{2};                         // source Keyboard selection
    std::uint32_t render_quality{100};                     // 43C060 default5DF5E4,439E50 ->60D73C
    std::int32_t width{1280}, height{720}, depth_bits{32}; // new platform drawable dimensions
    std::uint32_t audio_master_percent{100}, sound_percent{100}, music_percent{100}, audio_rate{22050};
    float master_gain{1};
    std::uint64_t window{};
    StartupObserver *startup_observer{}; // optional read-only qualification hook
};
void configure_session_audio(RaceSessionState &, const RaceSessionLaunch &); // full460C20 platform inputs

class RaceSession;
struct StartupObserver
{
    virtual ~StartupObserver() = default;
    virtual void before_race_initialization(const RaceSession &) = 0;
    virtual void after_race_initialization(const RaceSession &, const RaceInitializationObservation &) = 0;
};

struct SourceColdAttachmentOnly
{
};
enum class RaceSessionReadiness
{
    source_attachment_only,
    preview_constructing,
    preview_ready
};
class RaceSession
{
    struct Resources;
    std::unique_ptr<Resources> resources_;
    std::shared_ptr<RaceSessionState> state_;
    void require_preview_resources() const;
    RaceSession(PreviewHostEntry, std::filesystem::path, const RaceSessionLaunch &, SourceApplicationLiteralData *);

  public:
    // Aliases the existing platform control block. No new world, resources,
    // source reset, source child, backend preparation, or ready publication.
    RaceSession(SourceColdAttachmentOnly, std::shared_ptr<RaceSessionPlatform>);
    // Existing preview remains an explicit separate NEW host entry. It does
    // not reconstruct cold442120 or the complete scene442030 sequence.
    RaceSession(PreviewHostEntry, std::filesystem::path retail_root, const RaceSessionLaunch &);
    RaceSession(PreviewHostEntry, std::filesystem::path retail_root, const RaceSessionLaunch &,
                SourceApplicationLiteralData &);
    RaceSession(std::filesystem::path retail_root, const RaceSessionLaunch &);
    RaceSessionReadiness readiness() const noexcept;
    ~RaceSession();
    RaceSession(const RaceSession &) = delete;
    RaceSession &operator=(const RaceSession &) = delete;
    RaceSession(RaceSession &&) = delete; // borrowed level/model/audio identities stay stable
    RaceSession &operator=(RaceSession &&) = delete;
    RaceSessionState &state() noexcept
    {
        return *state_;
    }
    const RaceSessionState &state() const noexcept
    {
        return *state_;
    }
    const std::shared_ptr<RaceSessionState> &shared_state() const noexcept
    {
        return state_;
    }
    const std::shared_ptr<RaceSessionPlatform> &shared_platform() const noexcept;
    const AssetStore &assets() const;
    ModelResources &models();
    SceneResources &scene_resources();
    const SplineSet &splines() const;
    const contact::TrackGeometry &track_geometry() const;
    WindowsAudioDevice &audio_device();
    WindowsAudioStreamPlatform &audio_stream_platform();
    std::span<const std::uint8_t> track_pmd() const;
    TrackNormalView track_normal_view();
    std::span<const std::string> raw_texture_labels() const;
    const std::string &level_name() const;
    std::string_view text(unsigned source_index) const;
    std::uint64_t window() const;
    AudioDriver audio_view();
    VehicleResetParameters reset_parameters() const;
    std::array<std::uint32_t, 4> selected_vehicles() const;
};
} // namespace sgn::retail
