#pragma once
#include <cstdint>
namespace sgn::retail
{
struct SourceCrtStartupServices;
// Full4667E1, borrowing the existing CRT FILE table636A88/count637AA0.
// NativeSourceMenuCrt already implements SourceCrtStartupServices. Pass that
// actual retained owner, after its real startup; do not create another CRT.
// Returns a cleared FILE identity or0, without opening/reserving a descriptor.
std::uint32_t source_acquire_stream_4667e1(SourceCrtStartupServices &);
} // namespace sgn::retail
