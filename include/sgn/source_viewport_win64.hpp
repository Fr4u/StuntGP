#pragma once
#include <array>
#include <cstdint>

namespace sgn::retail
{
struct ViewportState;
enum class SourceViewportAdmission : std::uint32_t
{
    accepted = 0,
    control_mismatch = 1,
    unmasked = 2,
    reserved_precision = 3,
    occupied_stack = 4,
    nonzero_top = 5
};
struct SourceViewportStore
{
    std::uint32_t source_pc{}, value{};
};
// Optional NEW receiving data borrowed for this call, never a world/FPU owner.
// Values are read back after each actual store; no expected values enter ASM.
struct SourceViewportObservation
{
    std::array<std::uint64_t, 2> fistp64{};
    std::array<SourceViewportStore, 6> stores{};
    std::uint32_t store_count{};
    std::uint16_t entry_cw{}, exit_cw{}, entry_sw{}, exit_sw{};
    std::uint32_t entry_mxcsr{}, exit_mxcsr{};
};
// Full424BD0 value/store sequence on the SAME existing viewport. Native AMD64
// only: actual CW must match expected_cw, all masks set, PC24/53/64, empty TOP0.
// expected_cw validates inherited state; it does not select/replace that state.
// Admission rejection leaves viewport/observation and FP state unchanged.
// Successful return preserves CW/MXCSR and retains real x87 sticky effects.
// Abnormal SEH unwind restores entry CW, retaining partial stores and FP state.
// Valid exclusive viewport/observation memory and Session thread are required;
// unmasked trap, cold-path context and source fault-address parity are unproved.
SourceViewportAdmission set_viewport_origin_win64_424bd0(ViewportState &, std::uint32_t x_argument_bits,
                                                         std::uint32_t y_argument_bits, std::uint16_t expected_cw,
                                                         SourceViewportObservation *observation = nullptr);
} // namespace sgn::retail
