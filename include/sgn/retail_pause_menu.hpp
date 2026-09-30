#pragma once
#include "sgn/audio_stream_playback.hpp"
#include "sgn/retail_frontend_launch.hpp"
#include "sgn/retail_game_text.hpp"
#include "sgn/retail_pause_frame.hpp"
#include "sgn/retail_pause_lifecycle.hpp"
#include "sgn/retail_pause_meter.hpp"
#include "sgn/retail_race_initialization.hpp"
#include "sgn/retail_scene_transition.hpp"

namespace sgn::retail
{
class RaceOverlayTextures;
struct RaceTextureReplacement;
struct MenuKeyboardRawState;

// Exact24-byte source row. callback is a source function identity, never an
// executable host pointer. The seven supported identities are dispatched below.
struct PauseMenuRow
{
    std::uint32_t enabled{}, text_id{};
    std::int32_t x{}, y{};
    std::uint32_t callback{}, extra_text_id{};
};
static_assert(sizeof(PauseMenuRow) == 24);
std::array<PauseMenuRow, 8> initial_pause_menu_rows(); // 485120..4851DF, seed once

// Only the eight previously unrepresented edge bytes. The physical vehicle and
// kind words are borrowed from the existing FrontendInputBinding bank.
struct PauseMenuInputEdges
{
    std::uint8_t up{}, up_previous{}, down{}, down_previous{};
    std::uint8_t activate{}, activate_previous{}, back{}, back_previous{};
};
static_assert(sizeof(PauseMenuInputEdges) == 8);
// NEW HOST (R24..R29) pause ADVANCED row and page. Not recovered game behaviour
// and not a source object: host-only state beside the source menu words. R29:
// ADVANCED is drawn in the CAMERA row's slot (row 6) and opens a page with the
// original CAMERA row, the shared GRAPHICS rows and BACK. The source row table
// 485120..4851DF, every callback and 61B74C's domain stay unchanged (61B74C
// keeps 0..5 while ADVANCED is selected).
struct PauseGraphicsHostState
{
    bool row_selected{};           // ADVANCED highlighted; 61B74C keeps its source row
    bool panel_open{};             // ADVANCED page replaces the pause rows
    std::uint32_t panel_row{};     // page entry: [CAMERA] + GRAPHICS rows + BACK
    std::uint32_t first_visible{}; // first of the 7 page rows shown (page scrolls)
};
struct PauseGraphicsHostView
{
    PauseGraphicsHostState *state{};     // null: host row absent (exact original menu)
    const MenuKeyboardRawState *input{}; // left/right edge bytes +1A8/+1A9 (+1AF/+1B0 previous)
};
struct PauseMenuStateView
{
    std::uint32_t &release_count;            // 61B748;61B744 is lifecycle.physical_input
    std::uint32_t &selected_row;             // 61B74C
    std::uint32_t &highlight;                // 61B750
    std::uint32_t &overlay_ready;            // 61B754
    const std::uint32_t &requested_language; // 4835E0, distinct from loaded language
    std::span<PauseMenuRow, 8> rows;
    std::span<const FrontendInputBinding, 4> physical_bindings;
    std::span<const PauseMenuInputEdges, 4> physical_edges;
    PauseGraphicsHostView host_graphics{}; // NEW HOST (R24); default: no host row
};

// Only supplies borrowed invocation data. Restart itself always calls the full
// initialize_race port, never a virtual action/no-op notification. Construct the
// reset parameters from the live world at this boundary, after45F9D0 returned.
struct PauseMenuRestartInvocation
{
    RaceInitializationWorld world;
    RaceInitializationParameters parameters;
    // Existing VehicleResetWorld carries current by value. A receiving adapter
    // publishes that completed output to the sole source owner before60F2C8
    // and442350. This may alias world.reset.current in a standalone fixture.
    unsigned &current_vehicle_output;
};
struct PauseMenuRestartSource
{
    virtual ~PauseMenuRestartSource() = default;
    virtual PauseMenuRestartInvocation borrow_restart() = 0;
};
// Per-child acquisition prevents keeping material/alpha spans across earlier
// allocating children. Getters only borrow; they must not mutate game state.
// Each returned view remains valid throughout that complete synchronous child.
struct PauseMenuDrawingSources
{
    virtual ~PauseMenuDrawingSources() = default;
    virtual TextSubmission borrow_text() = 0;
    virtual std::span<PolygonSubmissionVertex> borrow_alpha_vertices() = 0;
};
// Reference-only presentation/input view. Actions run synchronously at the
// reached source callsite. No AudioDriver/RaceSession or callback-state copy.
struct PauseMenuActions
{
    virtual ~PauseMenuActions() = default;
    virtual void sound_461150(std::uint32_t sample) = 0;
    virtual void invoke(std::uint32_t source_identity) = 0;
};
struct PauseMenuDrawWorld
{
    PauseMenuStateView menu;
    const std::uint32_t &pending;
    const std::uint32_t &second_pause;
    const std::uint32_t &frame_pause;
    PausePhysicalInputIdentity &physical_input;
    const RaceClockState &clock;
    const std::uint32_t &split_setting;
    const std::uint32_t &render_b;
    const std::uint32_t &game_type;
    const std::uint32_t &detail;
    const std::uint32_t &selected_camera_preset;
    const LevelAudioState &level_audio;
    GameTextState &game_text;
    RaceTextureReplacement &textures;
    std::span<std::uint32_t, 35> descriptor;
    UiLineClipScratch &line_clip;
    PauseMeterLabels &meter_labels;
    PauseMenuDrawingSources &drawing;
    PolygonSubmissionSink &sink;
    PauseMenuActions &actions;
};
void dispatch_pause_menu_input_447100(PauseMenuDrawWorld &);
void submit_pause_menu_rows_446c20(PauseMenuDrawWorld &, float x, float y);
void submit_pause_menu_4473f0(PauseMenuDrawWorld &);

struct PauseMenuWorld
{
    PauseMenuStateView menu;
    PauseLifecycleWorld lifecycle;
    RaceClockState &clock;
    const std::uint32_t &render_b;         // 612960
    const std::uint32_t &game_type;        // 60DF98
    std::uint32_t &detail;                 // 61297C
    std::uint32_t &selected_camera_preset; // 60F020
    LevelOverrideOptions &options;
    AudioDriver &audio; // same LevelAudioState/Controls/split as lifecycle
    AudioStreamPlaybackWorld streaming;
    PauseMenuRestartSource &restart;
    SceneTransitionWorld &transition;
    SceneRaceResultWorld &results;
    VehicleStorage &vehicles;
    RaceFrameProjection frame_projection;
    GameTextState &game_text;
    RaceOverlayTextures &textures; // actual430D40 and source26 image
    std::span<std::uint32_t, 35> descriptor;
    UiLineClipScratch &line_clip;
    PauseMeterLabels &meter_labels;
    PauseMenuDrawingSources &drawing;
    PolygonSubmissionSink &sink;
};

// Explicit new host wiring check, not a source entry and not an automatic
// preamble on source early-return paths. Call when constructing the fixture or
// attaching an already qualified receiving adapter; it creates no owners.
void validate_pause_menu_bindings(const PauseMenuWorld &);

// Candidate full source ports. No Session owner/attachment or parent tick loop.
// All reference aliases must name the same live existing world. Children may
// change menu rows/settings at their actual external callback boundaries.
// Missing accessed text/input/row or callback identities throw a host guard;
// completed source stores/children are not rolled back. Navigation with seven
// disabled rows is rejected after seven visits (source would not terminate).
// Unknown callback identities are rejected only if the latched call is taken.
// Numeric child domains and synchronous, nonoverlapping storage apply.
void resume_pause_menu_446b40(PauseMenuWorld &); // detached member of447100
void restart_pause_menu_446af0(PauseMenuWorld &);
void exit_pause_menu_446b20(PauseMenuWorld &);
void set_pause_exit_marker_445350(LevelOverrideOptions &) noexcept;
void increment_pause_audio_b_446a90(PauseMenuWorld &) noexcept;
void increment_pause_audio_a_446ac0(PauseMenuWorld &) noexcept;
void increment_pause_detail_446a70(PauseMenuWorld &) noexcept;
void cycle_pause_camera_446bb0(PauseMenuWorld &);
void update_pause_camera_label_446b60(PauseMenuWorld &) noexcept;
void dispatch_pause_menu_input_447100(PauseMenuWorld &);
void submit_pause_menu_rows_446c20(PauseMenuWorld &, float x, float y);
void submit_pause_menu_4473f0(PauseMenuWorld &);
} // namespace sgn::retail
