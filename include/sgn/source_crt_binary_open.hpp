#pragma once
#include <cstdint>
#include <span>
namespace sgn::retail
{
struct SourceCrtStartupServices;
struct SourceCrtCloseImports;
// NEW host IAT transport only. NativeSourceMenuCrt implements this on its
// existing HANDLE map; there is no second FILE/descriptor/handle owner.
struct SourceCrtBinaryOpenImport
{
    virtual ~SourceCrtBinaryOpenImport() = default;
    virtual std::uint32_t create_file_a(std::span<const std::uint8_t> path, std::uint32_t access, std::uint32_t share,
                                        bool inherit, std::uint32_t disposition, std::uint32_t attributes) = 0;
};
// Full source leaves. They borrow the current tables and do not initialize CRT.
std::uint32_t source_acquire_descriptor_46b7a5(SourceCrtStartupServices &);
std::uint32_t source_publish_descriptor_handle_46b83a(SourceCrtStartupServices &, SourceCrtCloseImports &,
                                                      std::uint32_t descriptor, std::uint32_t native_identity);
// Admitted462AC3/462AA3->466671->46B9BF route: actual mode bytes rb\0,
// share40. NativeSourceMenuCrt supplies the same object for all three roles.
// Unsupported modes/missing bounded NUL reject before any source effect.
// Within this domain, real source failure returns0 FILE and retains errors.
std::uint32_t source_fopen_binary_read_462ac3(SourceCrtStartupServices &, SourceCrtCloseImports &,
                                              SourceCrtBinaryOpenImport &, std::span<const std::uint8_t> path,
                                              std::span<const std::uint8_t> mode);
// The separate bounded wb NUL caller shares the same descriptor/FILE body.
// Retains mutable creation-permission636538 and actual CREATE_ALWAYS effects.
std::uint32_t source_fopen_binary_write_462ac3(SourceCrtStartupServices &, SourceCrtCloseImports &,
                                               SourceCrtBinaryOpenImport &, std::span<const std::uint8_t> path,
                                               std::span<const std::uint8_t> mode);
} // namespace sgn::retail
