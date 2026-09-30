#pragma once
#include <cstdint>
namespace sgn::retail
{
// Borrow the same source FILE memory, CRT allocation/error owner and binary
// descriptor provider. These methods do not own a second FILE or buffer bank.
struct SourceCrtBufferServices
{
    virtual ~SourceCrtBufferServices() = default;
    virtual std::uint32_t word(std::uint32_t) = 0;
    virtual std::uint8_t byte(std::uint32_t) = 0;
    virtual void word(std::uint32_t, std::uint32_t) = 0;
    virtual void byte(std::uint32_t, std::uint8_t) = 0;
    virtual std::uint32_t allocate_462fcd(std::uint32_t) = 0;
    virtual std::uint32_t read_descriptor_binary_4690b9(std::uint32_t descriptor, std::uint32_t buffer,
                                                        std::uint32_t size) = 0;
};
// Full46B45E, including the counter increment and allocation-failure inline buffer.
void allocate_source_crt_buffer_46b45e(SourceCrtBufferServices &, std::uint32_t file);
// Full468FE0 control flow on the admitted binary descriptor provider. Writes
// FILE pointer/count/EOF/error in source order; does not implement fread463729.
std::uint32_t fill_source_crt_buffer_binary_468fe0(SourceCrtBufferServices &, std::uint32_t file);
} // namespace sgn::retail
