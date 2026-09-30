#pragma once
#include "sgn/race_session_platform.hpp"

namespace sgn::retail
{
// Full425AD0 tail ->4258A0, actual captured624488 allocation and sole
// 577650/654/660 owners. No TextureAnimationBank or copied PMD motion records.
// Source phase store precedes first reached record read; oscillation follows
// that read. Earlier source writes survive an invalid owner/extent boundary.
// Unrepresented static5898F8 is an explicit reached boundary, never empty.
void advance_source_texture_motion_425ad0(const std::shared_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
