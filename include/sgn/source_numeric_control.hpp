#pragma once
#include <cstdint>

namespace sgn::retail
{
// Exact original CRT translation, including reserved precision encodings.
// The source software flags are not native MXCSR bits or an x87 CW image.
std::uint32_t decode_source_x87_control_46308c(std::uint32_t raw) noexcept;
std::uint16_t encode_source_x87_control_46311e(std::uint32_t flags) noexcept;

// Two genuine instruction boundaries. Production supplies the concrete Win64
// FSTCW/FLDCW receiver below; no event-only device is used by the scheduler.
struct SourceX87Control
{
    virtual ~SourceX87Control() = default;
    virtual std::uint16_t fstcw() = 0;
    virtual void fldcw(std::uint16_t) = 0;
};
// Full463041: unconditional read, decode, mask merge, encode, write, then
// return the merged SOFTWARE flags. Mask0 still writes the re-encoded CW.
std::uint32_t update_source_x87_control_463041(SourceX87Control &, std::uint32_t value, std::uint32_t mask);
// Full429510 tail4294F0; two distinct read-modify-write calls, in source order.
void prepare_source_numeric_429510(SourceX87Control &);

// NEW Win64 SSE policy, deliberately distinct from recovered x87 behavior:
// mirror exception masks and rounding, preserve status flags, disable DAZ/FTZ.
std::uint32_t source_compatible_mxcsr(std::uint32_t prior, std::uint16_t x87_control) noexcept;
} // namespace sgn::retail
