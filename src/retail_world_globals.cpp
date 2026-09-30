#include "sgn/retail_world_globals.hpp"
#include <bit>

namespace sgn::retail
{
void initialize_world_frame_globals(WorldFrameGlobals &state) noexcept
{
    // 00401DE0..00401E49, including all three actual44EE10 identity writes.
    state.field_497f20 = {0, 125, -12500};
    state.field_497f70 = {0, 0, 0};
    state.field_497f80 = 1;
    state.field_497f84 = 100;
    for (auto &matrix : state.matrices)
        matrix = vehicle_identity();
}
void initialize_world_gravity(WorldGravity &state) noexcept
{
    // 00441F90..00442008. Reload the rounded magnitude before computing its
    // inverse and signed vertical component. The input factor is not clamped.
    state.unit.x = 0;
    state.unit.y = -1;
    state.unit.z = 0;
    state.inverse_factor = vehicle_f32(1.0 / double(state.factor));
    state.magnitude = vehicle_f32(double(state.factor) * std::bit_cast<float>(0x3e8b851fu));
    state.inverse_magnitude = vehicle_f32(1.0 / double(state.magnitude));
    state.vector = {0, vehicle_f32(double(state.magnitude) * -1.0), 0};
}
} // namespace sgn::retail
