#pragma once
#include <cstdint>
namespace sgn::retail
{
struct SourceCrtFileFlushServices
{
    virtual ~SourceCrtFileFlushServices() = default;
    virtual std::uint32_t read_word(std::uint32_t address, std::uint32_t site) = 0;
    virtual void write_word(std::uint32_t address, std::uint32_t value, std::uint32_t site) = 0;
    virtual std::uint32_t write_descriptor_4695e5(std::uint32_t descriptor, std::uint32_t buffer,
                                                  std::uint32_t size) = 0;
    virtual std::uint32_t commit_descriptor_46b968(std::uint32_t descriptor) = 0;
};
// Direct buffer flush: fclose462A4D does not enter the optional commit in466564.
std::uint32_t flush_source_crt_buffer_46659f(SourceCrtFileFlushServices &, std::uint32_t file);
std::uint32_t flush_source_crt_file_466564(SourceCrtFileFlushServices &, std::uint32_t file);
std::uint32_t flush_source_crt_streams_466604(SourceCrtFileFlushServices &, std::uint32_t mode);
} // namespace sgn::retail
