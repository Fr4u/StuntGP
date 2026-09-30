#pragma once
#include "sgn/source_save_page_draw.hpp"

namespace sgn::retail
{
class SourceFrontendMaterialBinding;
// Concrete host wiring above the existing menu children. All referents must
// outlive this layer; no language bytes or font metrics are retained here.
class SourceMenuLanguageChildren : public SourceSavePageDrawChildren
{
    friend class SourceApplicationCore;
    // Core alone replaces this pointer in its non-callback post-tail commit.
    // Every old binding remains owned by Core and permanently epoch-checked.
    SourceFrontendMaterialBinding *materials_;

  public:
    SourceMenuLanguageChildren(MenuKeyboardAcquisition &, Win32MenuDirectInputStartup &, CampaignScoreMusic &,
                               SessionOuterFrameServices &, PolygonSubmissionSink &, SourceFrontendMaterialBinding &,
                               std::uint8_t &);
    std::int32_t select_language_440c40(FrontendState &, std::uint32_t flag) final;
};
} // namespace sgn::retail
