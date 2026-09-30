#pragma once
#include "sgn/retail_model_draw_queue.hpp"
#include "sgn/retail_vehicle_strip.hpp"

namespace sgn::retail
{
// Full41D1C0 through42B7F0. Uses the live4864E8 template by reference and
// updates the sole474820 part transform's allocated matrix references.
bool append_vehicle_strip_draw(VehicleStripGeometry &, std::uint32_t quality, ModelTransform &live_template,
                               ModelDrawQueue &background, ViewMatrixArena &);
} // namespace sgn::retail
