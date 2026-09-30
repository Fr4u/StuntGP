#include "sgn/retail_hud_fraction.hpp"
#include "sgn/assets.hpp"
#include <cmath>

namespace sgn::retail
{
double hud_fraction_46291a(double operand)
{
    if (!std::isfinite(operand))
        throw FormatError("HUD fraction operand outside finite source domain");
    // The zero operand dispatches to465374: pop both values and FLDZ.
    if (operand == 0.0)
        return 0.0;
    return std::fmod(operand, 1.0);
}
} // namespace sgn::retail
