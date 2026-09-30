#pragma once
#include "sgn/retail_race_hud_submission.hpp"
#include "sgn/retail_normal_race_lifecycle.hpp"
#include "sgn/retail_quad_submission.hpp"

namespace sgn::retail
{
// Recovered results drawing; Session ownership is supplied by its adapter.
// All getters only borrow live existing owners, reacquiring after prior
// allocating children. Returned references must outlive the synchronous child.
struct ResultsDrawingSources
{
    virtual ~ResultsDrawingSources() = default;
    virtual TextSubmission borrow_text() = 0;
    virtual QuadSubmission borrow_quad() = 0;
    // Only App initial pointers use this receiver; generated car text stays
    // in the existing LevelCarSlot. No default or synthetic blank text.
    virtual std::span<const std::uint8_t> borrow_result_initial_text(std::uint32_t address,
                                                                     std::uint32_t source_site) = 0;
    virtual void submit_position_4487d0(PolygonSubmissionSink &, std::uint32_t index, float x, float y,
                                        std::uint32_t depth) = 0;
};
struct ResultsWorld
{
    RaceHudView hud;
    const VehicleStorage &vehicles;
    const unsigned &current_vehicle;       // 4D5BB8, not the controller's vehicle
    NormalRaceFinishState &results;        // 60D728 and actual four result-table identities
    const RaceRanking &ranking;            // 612D14 null sentinel differs from result_rows
    std::span<const LevelCarSlot, 6> cars; // four retained16-byte strings per car
    const GameTextState &game_text;
    const RaceClockState &clock;      // 443770 is a literal read of phase60F2B4
    RaceTextureReplacement &textures; // must support actual source27 -> slot15
    std::span<std::uint32_t, 35> descriptor;
    ResultsDrawingSources &drawing;
    PolygonSubmissionSink &sink;
};
struct ResultBoxParameters
{
    float x, y, depth, width, height;
    std::uint32_t flags;
};
// Candidate finite-number domain, explicit host guards for invalid owners,
// nonfinite/overflowing math and original uninitialized result selectors.
// Retained source stores are not rolled back on child failure. See PROPOSAL.md.
void prepare_result_layout_447fa0(RaceHudView);
void submit_result_box_4476c0(ResultsWorld &, ResultBoxParameters);
void submit_race_results_449fb0(ResultsWorld &);
} // namespace sgn::retail
