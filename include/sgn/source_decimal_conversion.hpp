#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace sgn::retail
{
class SourceApplicationLiteralRef;
class RaceSessionPlatform;
class NativeSourceMenuCrt;
// Caller-local source96-bit decimal value: low16, significand64, exponent/sign16.
// Not a global cache or an intermediate binary32/binary64 value.
using SourceExtendedDecimal12 = std::array<std::uint16_t, 6>;
// Shared existing46BD0D tail: digits46CD66 then46D4FB. The caller owns lexical
// semantics. Input digits are0..9, <=8 significant digits, power[-7,7].
SourceExtendedDecimal12 decode_source_decimal_digits_bounded(const SourceApplicationLiteralRef &,
                                                             std::span<const std::uint8_t> digits, int power,
                                                             bool negative);
// Existing frontend float packing and its domain checks are retained exactly.
std::uint32_t pack_source_frontend_float_bounded(const SourceApplicationLiteralRef &, const SourceExtendedDecimal12 &);
struct SourcePackedBinary64
{
    std::uint64_t bits;
    std::uint32_t status; // 46AD31 return:0 normal/zero,1 overflow,2 underflow
};
// Complete46AE9D/46AD31 packing with the actual4962A8 table. Input is source12
// bytes, not a host floating-point number; no decimal lexical domain implied.
SourcePackedBinary64 pack_source_binary64_46ae9d(const SourceApplicationLiteralRef &, const SourceExtendedDecimal12 &);
// Concrete bounded4631A7/4681B6 receiver over the actual Native CRT read/write API.
// Ordinary decimal, <=8 significant digits, <=5 fractional places, netpower[-7,7];
// no D/E exponent, non-ASCII or unterminated input. Unsupported input throws.
// Requires Core-bound CRT owning495DA8 and its real result region6365E0..6365F7.
// No new CRT storage is created here. Return represents4631EC's binary64 load.
std::uint64_t convert_source_local_atof_bounded_4631a7(RaceSessionPlatform &, NativeSourceMenuCrt &,
                                                       std::span<const std::uint8_t> written_local);
} // namespace sgn::retail

namespace sgn::retail
{
// Separate camera domain: <=12 significant digits, six fractional places,
// net power[-7,7]. Actual config.wad CAM values use <=11 digits/six places.
SourceExtendedDecimal12 decode_source_camera_digits_bounded(const SourceApplicationLiteralRef &,
                                                            std::span<const std::uint8_t>, int power, bool negative);
struct SourceBootSearchBoundary;
std::uint64_t convert_source_camera_atof_bounded_4631a7(RaceSessionPlatform &, NativeSourceMenuCrt &,
                                                        SourceBootSearchBoundary actual_bank_input);
} // namespace sgn::retail
