#pragma once
#include "sgn/retail_hud_layout_preparation.hpp"
#include "sgn/retail_text_submission.hpp"
#include "sgn/retail_audio_driver.hpp"
#include "sgn/retail_game_text.hpp"
#include "sgn/retail_vehicle_storage.hpp"

namespace sgn::retail
{
struct RaceTextureReplacement;
// Sole new mutable source owner:485258+12*i. Shared across controllers and
// retained across HUD/level initialization. Initial zeros are the pinned PE.
struct RaceHudStarPhases
{
    std::array<std::uint32_t, 10> phase{};
};
struct RaceHudSubmission
{
    RaceHudView hud;
    const VehicleStorage &vehicles;
    const RaceClockState &clock;
    const ProgressionRecordState &records;
    const RaceRanking &ranking;
    std::span<const LevelCarSlot, 6> cars;
    const GameTextState &game_text;
    const std::uint32_t &split_setting;
    const std::uint32_t &frame_pause;
    const RaceTextureReplacement &overlay_textures;
    RaceHudStarPhases &stars;
    std::span<std::uint32_t, 35> descriptor;        // same576C38 as loading/UI
    std::span<PolygonSubmissionVertex, 4> vertices; // same58D818; all UV1 retained
    TextSubmission rendering;                       // same fonts/material/cache/specular/viewport owners
    AudioDriver &audio;
};
// Full44AE40 parent. Run once at its source-step boundary; additional display
// refreshes must replay captured commands without advancing this mutable HUD.
// Only valid native owners and the qualified finite quad/numeric domain apply.
void submit_race_hud_44ae40(RaceHudSubmission, PolygonSubmissionSink &);
// Standalone source child: no parent HUD preparation/descriptor clear.
void submit_race_countdown_449b50(RaceHudSubmission, PolygonSubmissionSink &);
// Existing position glyph child required by full results449FB0.
void submit_race_position_4487d0(RaceHudSubmission, PolygonSubmissionSink &, std::uint32_t index, float x, float y,
                                 std::uint32_t depth);
// Reference-only leaf inputs for the source composer. No native AudioDriver or
// texture loader is required. The existing finite quad/numeric domain remains.
struct RaceHudLeafInputs
{
    RaceHudView hud;
    const VehicleStorage &vehicles;
    const RaceClockState &clock;
    const ProgressionRecordState &records;
    const RaceRanking &ranking;
    std::span<const LevelCarSlot, 6> cars;
    const GameTextState &game_text;
    const std::uint32_t &split_setting;
    const std::uint32_t &frame_pause;
    RaceHudStarPhases &stars;
    std::span<std::uint32_t, 35> descriptor;
    std::span<PolygonSubmissionVertex, 4> vertices;
    TextSubmission rendering;
};
// Same existing4487D0 glyph kernel, without an unused AudioDriver/texture view.
void submit_race_position_4487d0(RaceHudLeafInputs, PolygonSubmissionSink &, std::uint32_t index, float x, float y,
                                 std::uint32_t depth);
struct RaceHudSourceEnergy
{
    virtual ~RaceHudSourceEnergy() = default;
    virtual void energy_4493e0() = 0;
};
enum class RaceHudFiniteLeaf : std::uint32_t
{
    position = 0x448a70,
    timers = 0x4490e0,
    laps = 0x44a5c0,
    gauge = 0x44add0,
    score = 0x44adc0,
    progress = 0x44a800,
    gear = 0x4485e0,
    split_gauge = 0x44ae10,
    remaining = 0x44aad0
};
// Existing reached leaves only, WITHOUT44AE40 prefix/layout/other children.
// The mandatory source energy receiver prevents a native audio fallback.
void submit_race_hud_finite_leaf(RaceHudLeafInputs, RaceHudFiniteLeaf, RaceHudSourceEnergy &, PolygonSubmissionSink &);

} // namespace sgn::retail
