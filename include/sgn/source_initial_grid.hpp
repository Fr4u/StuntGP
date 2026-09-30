#pragma once
#include <cstdint>
namespace sgn::retail
{
class FrontendState;
// 4130D0, including actual428800 publication into571328 and both full grid bodies.
// Call only at the original sites after their source construction prefixes.
void initialize_source_grid_4130d0(FrontendState &, std::uint32_t selector);
// Direct412BF0 has a different null behavior: lookup/store followed by dereference.
void initialize_source_standard_grid_412bf0(FrontendState &);
// Contiguous40D549..40D593 only: grid, six actual4082D0 rigs, grid again.
// Neither the preceding40BEC0/tuning/HUD nor following41D980 is claimed here.
void initialize_source_race_grid_pair_40d549(FrontendState &);
} // namespace sgn::retail
