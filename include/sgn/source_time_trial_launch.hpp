#pragma once

namespace sgn::retail
{
class FrontendState;
// Full43A910 for the actual44DA17 callsite: ECX61C1D0, EDX61C1F0.
// Publishes selector3 and both retained borrows before43A720. Reads the actual
// input bank and ReplayBank header, then writes existing world-owned options,
// selected replay, input bindings and vehicle configuration in source order.
// No result clear, result copy, recording allocation, scene transition or loop
// activation. Caller44D740 must prepare input first and push Start25 afterward.
// Launch+18 is source32 address bits; result+8 remains439E90's native identity.
void prepare_frontend_time_trial_race_43a910(FrontendState &);
} // namespace sgn::retail
