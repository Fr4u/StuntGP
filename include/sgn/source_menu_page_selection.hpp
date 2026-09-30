#pragma once
#include "sgn/source_menu_navigation.hpp"
#include "sgn/retail_campaign_score_navigation.hpp"

namespace sgn::retail
{
struct SourcePageFactoryServices
{
    virtual ~SourcePageFactoryServices() = default;
    virtual std::uint32_t create_page_43e000(FrontendState &, std::uint32_t music_key, std::uint32_t page_kind) = 0;
};
std::uint32_t publish_frontend_page_43ee40(FrontendState &, std::uint32_t page, SourceNavigationPageServices &,
                                           CampaignScoreMusic &);
std::uint32_t select_frontend_page_43ef10(FrontendState &, std::uint32_t music_key, std::uint32_t page_kind,
                                          SourcePageFactoryServices &, SourceNavigationPageServices &,
                                          CampaignScoreMusic &);
std::uint32_t pop_frontend_page_43eef0(FrontendState &, SourceNavigationPageServices &, CampaignScoreMusic &);
void configure_frontend_inputs_43dc40(FrontendState &);
std::uint32_t create_frontend_title_page_43e000(FrontendState &, std::uint32_t music_key);

class SourceMenuPageSelectionChildren : public SourceMenuNavigationChildren, public SourcePageFactoryServices
{
    CampaignScoreMusic &music_;

  protected:
    // Complete remaining 43E000 case including its common music-key publication.
    virtual std::uint32_t create_other_page(FrontendState &, std::uint32_t music_key, std::uint32_t page_kind) = 0;
    virtual void close_other_page(FrontendState &, std::uint32_t object, std::uint32_t source_callback) = 0;

  public:
    SourceMenuPageSelectionChildren(MenuKeyboardAcquisition &keys, Win32MenuDirectInputStartup &input,
                                    CampaignScoreMusic &music)
        : SourceMenuNavigationChildren(keys, input), music_(music)
    {
    }
    std::uint32_t select_page_43ef10(FrontendState &, std::uint32_t, std::uint32_t) final;
    std::uint32_t create_page_43e000(FrontendState &, std::uint32_t, std::uint32_t) final;
    void close_page(FrontendState &, std::uint32_t object) final;
};
} // namespace sgn::retail
