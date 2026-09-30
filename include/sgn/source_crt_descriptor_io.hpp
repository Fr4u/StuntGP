#pragma once
#include <cstdint>
#include <span>
namespace sgn::retail
{
struct SourceCrtDescriptorIO
{
    virtual ~SourceCrtDescriptorIO() = default;
    virtual std::uint32_t word(std::uint32_t address) = 0;
    virtual std::uint8_t byte(std::uint32_t address) = 0;
    virtual void word(std::uint32_t address, std::uint32_t value) = 0;
    virtual void byte(std::uint32_t address, std::uint8_t value) = 0;
    // Actual IAT boundaries: DWORD guest handles must resolve to retained native handles.
    virtual bool write_file(std::uint32_t handle, std::uint32_t buffer, std::uint32_t size, std::uint32_t &written) = 0;
    virtual bool write_file_local(std::uint32_t handle, std::span<const std::uint8_t>, std::uint32_t &written) = 0;
    virtual bool flush_file_buffers(std::uint32_t handle) = 0;
    virtual std::uint32_t set_file_pointer(std::uint32_t handle, std::uint32_t offset, std::uint32_t method) = 0;
    virtual std::uint32_t last_error() = 0;
    virtual void map_os_error_4699d7(std::uint32_t error) = 0;
};
// 46B4A2 returns the raw device bit (0 or0x40). Its only validity gate is
// unsigned descriptor<637BC0: no open-bit check and no errno/doserrno writes.
std::uint32_t source_descriptor_device_46b4a2(SourceCrtDescriptorIO &, std::uint32_t);
std::uint32_t source_descriptor_handle_46b92b(SourceCrtDescriptorIO &, std::uint32_t);
std::uint32_t source_descriptor_seek_46993d(SourceCrtDescriptorIO &, std::uint32_t, std::uint32_t, std::uint32_t);
std::uint32_t source_descriptor_write_4695e5(SourceCrtDescriptorIO &, std::uint32_t, std::uint32_t, std::uint32_t);
// NEW HOST input view of the actual one-byte stack argument used by464712.
// Runs the SAME full4695E5 body; only its final IAT input transport differs.
// The span is synchronous and never retained. No invented guest stack address.
std::uint32_t source_descriptor_write_local_4695e5(SourceCrtDescriptorIO &, std::uint32_t descriptor,
                                                   std::span<const std::uint8_t, 1> argument_byte);
std::uint32_t source_descriptor_commit_46b968(SourceCrtDescriptorIO &, std::uint32_t);
} // namespace sgn::retail

namespace sgn::retail
{
struct SourceCrtStartupInfo
{
    std::uint16_t reserved_bytes;
    std::uint32_t reserved_pointer;
};
struct SourceCrtStartupServices : SourceCrtDescriptorIO
{
    virtual std::uint32_t allocate_462fcd(std::uint32_t size) = 0;
    virtual std::uint32_t calloc_46d0c0(std::uint32_t count, std::uint32_t width) = 0;
    virtual void fatal_46412d(std::uint32_t code) = 0;
    virtual SourceCrtStartupInfo get_startup_info_a() = 0;
    virtual std::uint32_t get_std_handle(std::uint32_t code) = 0;
    virtual std::uint32_t get_file_type(std::uint32_t handle) = 0;
    virtual void set_handle_count(std::uint32_t) = 0;
    virtual void invoke_startup_callback(std::uint32_t address) = 0;
    virtual void set_multibyte_codepage_46c823(std::uint32_t code) = 0;
};
void initialize_source_crt_descriptors_469792(SourceCrtStartupServices &);
void initialize_source_crt_streams_46b4c8(SourceCrtStartupServices &);
} // namespace sgn::retail

namespace sgn::retail
{
void initialize_source_crt_callbacks_462dcd(SourceCrtStartupServices &);
}
