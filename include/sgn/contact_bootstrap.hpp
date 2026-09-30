#pragma once
#include "sgn/contact_solver.hpp"

namespace sgn::retail::contact::solver
{
// Full 00408710: target is the loader's selected template (00474630), not
// implicitly the world's current vehicle (004D5BB8). Camera values are borrowed
// from the selected controller: position+08 and source forward+9C.
void place_template_from_camera(VehicleState &, State &, const CameraVector &position, const CameraVector &forward,
                                VehiclePayloadAudit *audit = nullptr);
} // namespace sgn::retail::contact::solver
