#pragma once

namespace sgn::retail
{
struct RailState;
class RaceSessionPlatform;

// 004192D0..004192E6: three unconditional DWORD stores, no borrowed-data read.
// Source 474670=-1, 4D5C9C=NULL, 4D5CA0=0, in that order.
void reset_rail_4192d0(RailState &) noexcept;

// NEW host thread guard; borrows the SAME platform world. Explicit leaf only:
// the 445FA0 / 4420D8 callers remain unattached.
void reset_platform_source_rail_4192d0(RaceSessionPlatform &);
} // namespace sgn::retail
