#pragma once
#include "sgn/source_menu_page_selection.hpp"
#include <cstdint>

namespace sgn::retail
{
// Full43C310, including unaligned unlock stores and final5DF5D4=1.
// Neither clears record flags nor copies the saved persistence comparison.
void set_source_unlocks_43c310(FrontendState &, std::uint8_t value);
// Full44D740 dispatcher0..12. The retained source5E3794 selects exactly one
// phase; out-of-range unsigned values return without changing source state.
// Uses the real phases4/5 launch/result owners and phase11 replay compressor.
// Integration requires PageEnterName9, Car10, Track11, Start25, Save27,
// HighScore39 and PageUnlock40. SourceMainMenuChildren routes the actual mode entry.
// The existing Start/scene/warm-loader boundaries must propagate unchanged.
void advance_frontend_time_trial_mode_44d740(FrontendState &, SourcePageFactoryServices &,
                                             SourceNavigationPageServices &, CampaignScoreMusic &);
} // namespace sgn::retail
