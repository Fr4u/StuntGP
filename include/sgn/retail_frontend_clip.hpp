#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace sgn::retail
{
struct FrontendSourceStorage;
struct WorldMaterialPreludeEntry;
// Required462860 value semantics after FLD of an IEEE754 binary32 source word.
// Signed64 FISTP result represented as unsigned bits, including8000000000000000
// for masked invalid conversion. This is not a generic80-bit ST0/x87 emulator.
// Source exception masks must be set; source FP status flags/traps are outside
// this value API. Integer-only C++ leaves the host floating environment untouched.
std::uint64_t frontend_ftol_f32_462860(std::uint32_t source_float_bits) noexcept;

// Full438190 output/control-flow semantics for four binary32 stack arguments,
// with masked source FP exceptions and valid stack/FPU depth. Stores source
// EAX words to5DA360/364/368/36C; does not mutate a display viewport or registry.
// Argument copies may be rewritten by the source, but caller argument storage
// is popped by RET16 and is not an output of this host API.
void set_frontend_clip_438190(FrontendSourceStorage &, std::array<std::uint32_t, 4> argument_bits) noexcept;

// Full441190: existing prefix441190..4411AC followed by actual4411C0 tail.
// The older bounded prefix remains available unchanged for its immutable oracle.
void initialize_frontend_materials_441190(FrontendSourceStorage &, std::span<const WorldMaterialPreludeEntry>);
} // namespace sgn::retail
