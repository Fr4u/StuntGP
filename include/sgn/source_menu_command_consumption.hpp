#pragma once
#include "sgn/source_menu_command_queue.hpp"
#include "sgn/session_outer_frame_services.hpp"
#include "sgn/retail_text_submission.hpp"

namespace sgn::retail
{
// Invocation borrows only. Source words remain in FrontendState/world owners.
struct SourceMenuSubmission
{
    FrontendState &frontend;
    SessionOuterFrameServices &frames;
    PolygonSubmissionSink &sink;
    RaceTextureReplacement &textures;
};
void consume_frontend_commands_437ec0(SourceMenuSubmission &);
void consume_frontend_lists_437ed0(SourceMenuSubmission &);
void consume_frontend_command_437f40(SourceMenuSubmission &, std::uint32_t command, std::uint32_t list);
std::int32_t resolve_frontend_material_441240(SourceMenuSubmission &, std::span<std::uint32_t, 12>);
class SourceMenuConsumptionChildren : public SourceMenuCommandQueueChildren
{
    friend class SourceApplicationCore;
    SessionOuterFrameServices &frames_;
    PolygonSubmissionSink &sink_;
    RaceTextureReplacement *textures_; // Core-only atomic adapter commit; referent stays owned
  public:
    SourceMenuConsumptionChildren(MenuKeyboardAcquisition &, Win32MenuDirectInputStartup &, CampaignScoreMusic &,
                                  SessionOuterFrameServices &, PolygonSubmissionSink &, RaceTextureReplacement &);
    void submit_lists_437ec0(FrontendState &) final;
};
} // namespace sgn::retail
