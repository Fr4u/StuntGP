#include "sgn/retail_hud_layout_preparation.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
constexpr std::uint64_t sign64 = 0x8000000000000000ull, exponent64 = 0x7ff0000000000000ull,
                        quiet64 = 0x0008000000000000ull;
constexpr double indefinite()
{
    return std::bit_cast<double>(sign64 | exponent64 | quiet64);
}
// x87 FLD float quiets a signaling NaN while preserving its sign/payload.
// Memory operands of FADD are different: classification must precede that
// quieting. This is source IEEE behavior, not an approximation tolerance.
double load_float(float value)
{
    const auto b = std::bit_cast<std::uint32_t>(value);
    if ((b & 0x7fffffffu) > 0x7f800000u)
        return std::bit_cast<double>((std::uint64_t(b & 0x80000000u) << 32) | exponent64 |
                                     (std::uint64_t(b & 0x7fffffu) << 29) | quiet64);
    return double(value);
}
double add_float(double left, float right)
{
    const auto a = std::bit_cast<std::uint64_t>(left);
    const auto b = std::bit_cast<std::uint32_t>(right);
    const auto ma = a & ~sign64;
    const auto mb = b & 0x7fffffffu;
    const bool an = ma > exponent64, bn = mb > 0x7f800000u;
    if (an || bn)
    {
        const std::uint64_t converted =
            (std::uint64_t(b & 0x80000000u) << 32) | exponent64 | (std::uint64_t(b & 0x7fffffu) << 29);
        auto chosen = an ? a : converted;
        if (an && bn)
        {
            const bool aq = (a & quiet64) != 0, bq = (b & 0x400000u) != 0;
            const auto other = converted & ~sign64;
            if (aq != bq)
                chosen = aq ? a : converted;
            else
                chosen = ma == other ? (a < converted ? a : converted) : ma > other ? a : converted;
        }
        return std::bit_cast<double>(chosen | quiet64);
    }
    if (ma == exponent64 && mb == 0x7f800000u && ((a >> 63) != (b >> 31)))
        return indefinite();
    return left + double(right);
}
double scaled_offset(std::uint32_t raw, double factor)
{
    const double signed_value = std::bit_cast<std::int32_t>(raw); // literal signed FILD, no prior float conversion
    if (std::isnan(factor))
        return factor;
    if (signed_value == 0 && std::isinf(factor))
        return indefinite();
    const double product = signed_value * factor;
    return product * double(std::bit_cast<float>(0x3c23d70au)); // PE46E294, exact float0.01
}
RaceHudLayout &selected(RaceHudView view)
{
    if (view.selected_hud >= 4)
        throw FormatError("HUD layout preparation selected identity outside four owners");
    return view.layouts[view.selected_hud];
}
double clamped_displacement(float value)
{
    const double loaded = load_float(value);
    if (loaded > 128.0)
        return 128.0;
    // 44ACC7..44ACD7/44ACF5..44AD04: unordered takes the lower clamp.
    if (!(loaded >= -128.0))
        return -128.0;
    return loaded;
}
} // namespace
void prepare_hud_layout_447d80(RaceHudView view)
{
    auto &hud = selected(view);
    const auto selected_identity = view.selected_hud;
    // Literal unrolled source order. ECX caches the selected HUD at447D80.
    for (const unsigned element : {1u, 2u, 7u, 3u, 4u, 5u, 6u})
    {
        auto factor = add_float(load_float(hud.value[element - 1]), view.countdown[selected_identity].value);
        if (factor > 100.0)
            factor = 100.0; // upper clamp only; unordered retains NaN
        const auto &input = hud.elements[element];
        auto &output = hud.elements[element - 1];
        output.word_1c =
            std::bit_cast<std::uint32_t>(static_cast<float>(add_float(scaled_offset(input.word_14, factor), input.x)));
        output.word_20 =
            std::bit_cast<std::uint32_t>(static_cast<float>(add_float(scaled_offset(input.word_18, factor), input.y)));
    }
}
void apply_hud_displacement_44ac90(RaceHudView view, std::span<const VehicleState, 6> vehicles)
{
    // New host identity guards precede every recovered store.
    if (view.controls.current >= 4)
        throw FormatError("HUD displacement current controller outside four owners");
    const auto target = view.controls.controllers[view.controls.current].vehicle;
    if (target >= vehicles.size())
        throw FormatError("HUD displacement current vehicle outside six owners");
    auto &hud = selected(view);
    const auto &source = vehicles[target].auxiliary_displacement;
    // 44AC9F/44ACA5 read both source fields before any destination writes.
    const double x = clamped_displacement(source.x), y = clamped_displacement(source.y);
    for (unsigned element = 0; element < 7; ++element)
    {
        auto &output = hud.elements[element];
        output.word_1c =
            std::bit_cast<std::uint32_t>(static_cast<float>(add_float(x, std::bit_cast<float>(output.word_1c))));
        output.word_20 =
            std::bit_cast<std::uint32_t>(static_cast<float>(add_float(y, std::bit_cast<float>(output.word_20))));
    }
}
} // namespace sgn::retail
