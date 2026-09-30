#pragma once
#include "sgn/vehicle_score.hpp"
#include "sgn/retail_quad_submission.hpp"

namespace sgn::retail
{
struct ScorePopupSubmission
{
    const VehicleScoreState &score;                 // same Session popup bank496990, no copy/second owner
    const contact::player::Controls &controls;      // live current pointer4722D0 and+AC vehicle
    std::span<PolygonSubmissionVertex, 4> vertices; // shared58D818
    std::uint32_t &selected_material;               // shared61C5E4
    std::span<const PolygonSubmissionMaterial> materials;
    const ViewportState &viewport;
};
// Full401B00: reverse traversal, source atlas and full432E00 backend. Finite
// quad arithmetic/index domain applies. Input bank is retained; descriptors are
// local35-word copies. Active eligible slots require a valid current controller.
// Text reads use the complete existing score state representation, including enabled
// after the eight text bytes. A read outside the owner throws a host guard.
void submit_score_popups_401b00(ScorePopupSubmission, PolygonSubmissionSink &);
} // namespace sgn::retail
