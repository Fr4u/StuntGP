#pragma once
#include <cstdint>

namespace sgn::retail
{
// Concrete production429510 receiver. Reads/writes the calling thread's REAL
// x87 control word through two MASM leaf functions; synchronizes real MXCSR.
// No process/world mirror or new timer. Its documented purpose is to change
// nonvolatile FPCSR/MXCSR control state for subsequent source computations.
// Caller is the Session owner thread; the scheduler verifies that before this
// call. Native support here is Windows AMD64, all x87 exceptions masked.
// An unmasked environment is rejected before a control write. Native faults
// from an already pending unmasked x87 exception are not translated here.
void prepare_source_numeric_win64_429510();
} // namespace sgn::retail

// Private ABI declarations shared with the native receiving. These leaf
// helpers intentionally change/read FPCSR and use caller-provided home space.
extern "C" std::uint16_t sgn_source_read_x87_control() noexcept;
extern "C" void sgn_source_write_x87_control(std::uint16_t) noexcept;
