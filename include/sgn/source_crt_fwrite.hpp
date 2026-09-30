#pragma once
#include <cstdint>
namespace sgn::retail
{
struct SourceCrtDescriptorIO;
struct SourceCrtBufferServices;
struct SourceCrtFileFlushServices;
// All service references below must be views of the SAME retained CRT owner,
// like close462A4D: one FILE/descriptor memory, allocator and shared errno.
// Full46462A, including its source stack-byte output through the actual4695E5.
std::uint32_t flush_source_crt_byte_46462a(SourceCrtDescriptorIO &, SourceCrtBufferServices &, std::uint32_t value,
                                           std::uint32_t file);
// Full463811 over existing copy4692B0, direct flush46659F and descriptor4695E5.
// Product and pointer arithmetic are source DWORD operations. Partial effects
// survive failure; the return is completed elements, not bytes or fclose status.
// This does not open a file, attach replay save or provide a new source buffer.
std::uint32_t write_source_crt_elements_463811(SourceCrtDescriptorIO &, SourceCrtBufferServices &,
                                               SourceCrtFileFlushServices &, std::uint32_t source,
                                               std::uint32_t element_size, std::uint32_t element_count,
                                               std::uint32_t file);
} // namespace sgn::retail
