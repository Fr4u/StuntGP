#pragma once
#include <cstddef>
#include <cstdint>

namespace sgn::retail
{
class RaceSessionPlatform;
class SourceAllocationBank;
class SourceAllocationRef;
struct SourceBootFileState;

enum class SourceWadAccessKind
{
    read,
    write
};
struct SourceWadTransferAccess
{
    SourceWadAccessKind kind;
    std::uint32_t source_site, source_offset, destination_offset, width, value;
    std::uint64_t ordinal;
    bool operator==(const SourceWadTransferAccess &) const = default;
};
struct SourceWadTransferObserver
{
    virtual ~SourceWadTransferObserver() = default;
    // NEW diagnostic after a completed logical access. Read-only and
    // non-reentrant: may throw, but must not mutate/free either owner, the
    // borrowed destination, or copied. Offsets are not x86 or host addresses.
    virtual void after_access(const SourceWadTransferAccess &, const SourceBootFileState &) = 0;
};

// Bounded WAD transfer leaf: source44B338..44B365, including unsigned min,
// DWORD44B354 then BYTE44B35D, returning the original member length.
// Inputs are source register/local values, not a resume capability. This
// function does not route paths, load archives, open a file or execute44B260.
// A future caller must reach the actual successful WAD-lookup branch first.
// The actual platform supplies both its existing raw cache and bank. No
// allocation/boot/release is performed; a null/expired reference is checked
// only at a reached destination write. A zero transfer does not borrow it.
std::uint32_t transfer_platform_source_wad_44b354(RaceSessionPlatform &, const SourceAllocationRef &destination,
                                                  std::uint32_t member_offset, std::uint32_t requested,
                                                  std::uint32_t member_length, std::size_t &copied,
                                                  SourceWadTransferObserver *observer = nullptr);

namespace detail
{
// Internal concrete-owner binding used by the platform wrapper. No generic
// memory provider, address-space emulation or alternative source buffer.
std::uint32_t transfer_source_wad_to_bank_44b354(const SourceBootFileState &, SourceAllocationBank &,
                                                 const SourceAllocationRef &, std::uint32_t member_offset,
                                                 std::uint32_t requested, std::uint32_t member_length,
                                                 std::size_t &copied, SourceWadTransferObserver *);
} // namespace detail
} // namespace sgn::retail
