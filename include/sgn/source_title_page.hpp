#pragma once
#include "sgn/source_menu_page_selection.hpp"

namespace sgn::retail
{
struct SourceTitlePageServices
{
    virtual ~SourceTitlePageServices() = default;
    // Sole existing process word. Requested only when the idle-demo branch runs.
    // An unbound owner is an explicit missing child, never a private counter.
    virtual std::uint32_t &demo_index_627fdc(FrontendState &) = 0;
    virtual void load_demo_43b0e0(FrontendState &, std::uint32_t record_address) = 0;
    virtual void feedback_440890(FrontendState &, std::uint32_t) = 0;
    virtual void sprite_438710(FrontendState &, std::uint32_t ecx, std::uint32_t edx,
                               std::array<std::uint32_t, 4> stack) = 0;
    virtual void text_438ff0(FrontendState &, std::uint32_t ecx, std::uint32_t edx,
                             std::array<std::uint32_t, 11> stack) = 0;
    virtual void text_438ba0(FrontendState &, std::uint32_t ecx, std::uint32_t edx,
                             std::array<std::uint32_t, 6> stack) = 0; // discard original ST0 return
};
std::uint32_t push_frontend_page_43dfa0(FrontendState &, std::uint32_t music_key, std::uint32_t kind,
                                        SourcePageFactoryServices &, SourceNavigationPageServices &,
                                        CampaignScoreMusic &);
void reset_frontend_title_page_45c050(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_title_page_45c080(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                  std::uint32_t argument, SourcePageFactoryServices &,
                                                  SourceNavigationPageServices &, CampaignScoreMusic &,
                                                  SourceTitlePageServices &);
class SourceMenuTitleChildren : public SourceMenuPageSelectionChildren, public SourceTitlePageServices
{
    CampaignScoreMusic &title_music_;

  protected:
    virtual void other_page_event(FrontendState &, std::uint32_t page, std::uint32_t callback, std::uint32_t event,
                                  std::uint32_t argument) = 0;

  public:
    SourceMenuTitleChildren(MenuKeyboardAcquisition &keys, Win32MenuDirectInputStartup &input,
                            CampaignScoreMusic &music)
        : SourceMenuPageSelectionChildren(keys, input, music), title_music_(music)
    {
    }
    void page_event(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) final;
};
} // namespace sgn::retail
// Windows x64 leaf: inputs={fraction bits,slide bits}, output=source y float bits.
// Executes the original x87 FSIN and following operations; requires the source
// numeric environment established by 441F00 and two free x87 stack registers.
extern "C" void sgn_title_y_45c1eb(const std::uint32_t *, std::uint32_t *) noexcept;
