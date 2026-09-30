#pragma once
#include "sgn/polygon_submission.hpp"
#include "sgn/sdr_gamma.hpp"
#include <utility>

namespace sgn
{
// NEW host ownership only: index0 is front, index1 is back. No second bank.
// One instance belongs to the actual application backend; the stable surface
// adapter borrows it. External GPU recipe copies must retire before suspension.
struct PolygonSurfaceRecipes
{
    std::array<ImmutablePolygonSubmission, 2> submissions;
    std::array<SdrGammaRamp, 2> ramps{identity_sdr_gamma(), identity_sdr_gamma()};
    void exchange() noexcept
    {
        submissions[0].swap(submissions[1]);
        ramps[0].swap(ramps[1]);
    }
};
} // namespace sgn
