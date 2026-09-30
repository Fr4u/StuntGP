#pragma once
#include "sgn/source_crt_descriptor_io.hpp"
#include "sgn/source_crt_file_flush.hpp"
#include <cstdint>
namespace sgn::retail
{
// Imports and paired allocation transport belong to the SAME retained CRT as
// the descriptor/FILE memory below. No independent HANDLE or errno registry.
struct SourceCrtCloseImports
{
    virtual ~SourceCrtCloseImports() = default;
    virtual bool close_native_handle(std::uint32_t handle) = 0;
    virtual bool set_std_handle(std::uint32_t code, std::uint32_t handle) = 0;
    // Paired with that owner's allocate_462fcd/calloc_46d0c0. Native uses
    // NEW host allocation release, not reconstructed original heap internals.
    virtual void free_462f9e(std::uint32_t address) = 0;
};
std::uint32_t source_descriptor_invalidate_46b8b1(SourceCrtDescriptorIO &, SourceCrtCloseImports &,
                                                  std::uint32_t descriptor);
std::uint32_t source_descriptor_close_466486(SourceCrtDescriptorIO &, SourceCrtCloseImports &,
                                             std::uint32_t descriptor);
void free_source_crt_buffer_466539(SourceCrtDescriptorIO &, SourceCrtCloseImports &, std::uint32_t file);
std::uint32_t close_source_crt_file_462a4d(SourceCrtDescriptorIO &, SourceCrtFileFlushServices &,
                                           SourceCrtCloseImports &, std::uint32_t file);
} // namespace sgn::retail
