#pragma once
#include "sgn/source_menu_widget_draw.hpp"
#include "sgn/source_selection_page_navigation.hpp"
namespace sgn::retail
{
// Extends the current hierarchy; all selection draw children use its same queue.
class SourceSelectionPageDrawChildren : public SourceMenuWidgetDrawChildren, public SourceSelectionPageServices
{
    CampaignScoreMusic &selection_music_;

  protected:
    virtual void remaining_selection_page_event(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t,
                                                std::uint32_t) = 0;
    void remaining_page_event(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) final;

  public:
    SourceSelectionPageDrawChildren(MenuKeyboardAcquisition &, Win32MenuDirectInputStartup &, CampaignScoreMusic &,
                                    SessionOuterFrameServices &, PolygonSubmissionSink &, RaceTextureReplacement &,
                                    std::uint8_t &);
    void finalize_track_45c670(FrontendState &, std::uint32_t) final;
    std::uint32_t track_value_437da0(FrontendState &, std::uint32_t) final;
    void clip_438190(FrontendState &, std::array<std::uint32_t, 4>) final;
    void track_strip_45cba0(FrontendState &, std::uint32_t, std::uint32_t) final;
    void track_title_45cc40(FrontendState &, std::uint32_t) final;
    void track_details_45cc70(FrontendState &, std::uint32_t) final;
    void track_player_45d130(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) final;
    std::uint32_t displayed_car_451e80(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) final;
    void car_strip_452070(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) final;
    void car_frame_452130(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) final;
    void car_details_4521b0(FrontendState &, std::uint32_t, std::uint32_t) final;
    void car_player_452000(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) final;
    void car_stats_451ed0(FrontendState &, std::uint32_t) final;
};
} // namespace sgn::retail
