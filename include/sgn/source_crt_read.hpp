#pragma once
#include "sgn/source_crt_descriptor_io.hpp"
#include <stdexcept>
namespace sgn::retail
{
// NEW host import boundary. Guest buffers and handles must resolve to the same
// retained CRT owners. ReadFile writes the actual destination, including any
// partial prefix, before the return is interpreted by the source caller.
struct SourceCrtReadFileImport
{
    virtual ~SourceCrtReadFileImport() = default;
    virtual bool read_file(std::uint32_t handle, std::uint32_t buffer, std::uint32_t size,
                           std::uint32_t &transferred) = 0;
};
class SourceCrtReadGap final : public std::runtime_error
{
  public:
    const std::uint32_t source_site, source_value;
    SourceCrtReadGap(std::uint32_t site, std::uint32_t value, const char *message)
        : std::runtime_error(message), source_site(site), source_value(value)
    {
    }
};
// 4690B9 descriptor read, admitted binary path only. The descriptor table,
// lookahead byte and App errno/doserrno are the existing SourceCrtDescriptorIO
// owners. Invalid-descriptor, zero-count and descriptor-EOF source branches are
// retained. Text translation at469199 is an explicit gap, not a raw-read fallback.
std::uint32_t source_descriptor_read_binary_4690b9(SourceCrtDescriptorIO &, SourceCrtReadFileImport &,
                                                   std::uint32_t descriptor, std::uint32_t buffer, std::uint32_t size);
} // namespace sgn::retail
