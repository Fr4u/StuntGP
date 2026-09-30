#include "sgn/retail_frontend_clip.hpp"
#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_world_material_loading.hpp"

namespace sgn::retail
{
namespace
{
constexpr std::uint32_t sign_bit = 0x80000000u, magnitude_mask = 0x7fffffffu;
bool unordered(std::uint32_t bits) noexcept
{
    return (bits & magnitude_mask) > 0x7f800000u;
}
std::uint32_t clamp_lower_source(std::uint32_t bits) noexcept
{
    // 438193..4381BA: FCOMP sets C0 for less-than AND unordered.
    const bool negative_nonzero = (bits & sign_bit) != 0 && (bits & magnitude_mask) != 0;
    return unordered(bits) || negative_nonzero ? 0u : bits;
}
std::uint32_t clamp_upper_source(std::uint32_t bits, std::uint32_t maximum) noexcept
{
    // 4381C1..4381E8: TEST AH,41 accepts equal/less/unordered unchanged.
    // Positive ordered binary32 magnitudes have the same order as their bits.
    return (bits & sign_bit) == 0 && !unordered(bits) && bits > maximum ? maximum : bits;
}
} // namespace
std::uint64_t frontend_ftol_f32_462860(std::uint32_t bits) noexcept
{
    constexpr std::uint64_t indefinite = 0x8000000000000000ull;
    const auto exponent = (bits >> 23) & 255u;
    const auto fraction = bits & 0x7fffffu;
    const bool negative = (bits & sign_bit) != 0;
    if (exponent == 255u)
        return indefinite; // NaN/infinity: masked FISTP64 invalid
    if (exponent < 127u)
        return 0; // signed zero, subnormal, magnitude below1
    // Unbiased exponent63 admits exactly-2^63; +2^63 and larger are invalid.
    // Its valid negative result and invalid result have the same integer bits.
    if (exponent >= 190u)
        return indefinite;
    const auto power = exponent - 127u; // 0..62; no out-of-range shift
    const auto significand = std::uint64_t{0x800000u | fraction};
    const auto magnitude = power >= 23u ? significand << (power - 23u) : significand >> (23u - power);
    // Modulo64 negation preserves the source two's-complement result, with no
    // float-to-signed conversion, signed overflow or signed right shift.
    return negative ? std::uint64_t{0} - magnitude : magnitude;
}
void set_frontend_clip_438190(FrontendSourceStorage &frontend, std::array<std::uint32_t, 4> bits) noexcept
{
    bits[0] = clamp_lower_source(bits[0]);
    bits[1] = clamp_lower_source(bits[1]);
    bits[2] = clamp_upper_source(bits[2], 0x44200000u);
    bits[3] = clamp_upper_source(bits[3], 0x43f00000u);
    // 4381F2..43821E: only low EAX is published after each real conversion.
    for (std::size_t i = 0; i < 4; ++i)
        frontend.frontend_clip[i] = static_cast<std::uint32_t>(frontend_ftol_f32_462860(bits[i]));
}
void initialize_frontend_materials_441190(FrontendSourceStorage &frontend,
                                          std::span<const WorldMaterialPreludeEntry> materials)
{
    (void)initialize_frontend_material_prefix_441190(frontend, materials);
    initialize_frontend_lookup_tables(frontend); // 4411AD JMP4411C0; RET to437E6A
}
} // namespace sgn::retail
