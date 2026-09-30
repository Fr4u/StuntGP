#pragma once
#include "sgn/source_single_page_navigation.hpp"
namespace sgn::retail
{
// Each required method corresponds to one actual source call. Drawing must use
// the same command queue/resource owner as MenuText; no empty implementations.
struct SourceSelectionPageServices
{
    virtual ~SourceSelectionPageServices() = default;
    virtual void finalize_track_45c670(FrontendState &, std::uint32_t page) = 0;
    virtual std::uint32_t track_value_437da0(FrontendState &, std::uint32_t track) = 0;
    virtual void clip_438190(FrontendState &, std::array<std::uint32_t, 4> stack) = 0;
    virtual void track_strip_45cba0(FrontendState &, std::uint32_t selected, std::uint32_t phase) = 0;
    virtual void track_title_45cc40(FrontendState &, std::uint32_t track) = 0;
    virtual void track_details_45cc70(FrontendState &, std::uint32_t page) = 0;
    virtual void track_player_45d130(FrontendState &, std::uint32_t page, std::uint32_t x, std::uint32_t y) = 0;
    virtual std::uint32_t displayed_car_451e80(FrontendState &, std::uint32_t page, std::uint32_t x,
                                               std::uint32_t y) = 0;
    virtual void car_strip_452070(FrontendState &, std::uint32_t car, std::uint32_t x, std::uint32_t y) = 0;
    virtual void car_frame_452130(FrontendState &, std::uint32_t page, std::uint32_t x, std::uint32_t y,
                                  std::uint32_t phase) = 0;
    virtual void car_details_4521b0(FrontendState &, std::uint32_t page, std::uint32_t car) = 0;
    virtual void car_player_452000(FrontendState &, std::uint32_t page, std::uint32_t x, std::uint32_t y) = 0;
    virtual void car_stats_451ed0(FrontendState &, std::uint32_t page) = 0;
};
std::uint32_t dispatch_frontend_track_page_45c6a0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                  std::uint32_t argument, SourcePageFactoryServices &,
                                                  SourceNavigationPageServices &, CampaignScoreMusic &,
                                                  SourceMainPageServices &, SourceSelectionPageServices &);
std::uint32_t dispatch_frontend_car_page_451aa0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                std::uint32_t argument, SourceNavigationPageServices &,
                                                CampaignScoreMusic &, SourceMainPageServices &,
                                                SourceSelectionPageServices &);
} // namespace sgn::retail
