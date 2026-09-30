#pragma once

namespace sgn::retail
{
// HUD-specific46291A contract: a finite binary64 operand formed by the
// original53-bit x87 multiply, with the pinned denominator1 from46E2C8.
// This is not a general CRT replacement. NaN/infinity and enabled historical
// processor-erratum/FP-exception routes are outside this interface.
double hud_fraction_46291a(double operand);
} // namespace sgn::retail
