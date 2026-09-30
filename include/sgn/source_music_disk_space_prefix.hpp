#pragma once
#include "sgn/source_music_drive_type_prefix.hpp"

namespace sgn::retail
{
enum class SourceMusicDiskWordOrigin : std::uint8_t
{
    original_argument_pointer,
    unwritten_stack,
    provider_write
};
// Absence is unknown source bits, never a source DWORD initialized to zero.
struct SourceMusicDiskWordState
{
    SourceMusicDiskWordOrigin origin;
    std::optional<std::uint32_t> bits;
};

// Synchronous, checked access to one of the two actual receiving frame cells.
// Four arguments use these two cells; sectors/free/total share one cell.
// No raw DWORD pointer is exposed. This is not an installed Win32 adapter.
class SourceMusicDiskDwordAccess final
{
    struct Cell;
    struct Scope;
    enum class Slot : std::uint8_t
    {
        argument,
        bytes_per_sector
    };
    std::shared_ptr<Scope> scope_;
    Slot slot_;
    SourceMusicDiskDwordAccess(std::shared_ptr<Scope>, Slot);
    std::shared_ptr<RaceSessionPlatform> checked_owner() const;
    friend struct SourceMusicPrefixExecution;

  public:
    SourceMusicDiskDwordAccess() = delete;
    SourceMusicDiskDwordAccess(const SourceMusicDiskDwordAccess &) = default;
    SourceMusicDiskDwordAccess &operator=(const SourceMusicDiskDwordAccess &) = default;
    ~SourceMusicDiskDwordAccess() = default;
    SourceMusicDiskWordState read() const;
    void write(std::uint32_t bits) const;
    bool aliases(const SourceMusicDiskDwordAccess &other) const;
};

class SourceMusicDiskFreeSpaceReceiving
{
  public:
    virtual ~SourceMusicDiskFreeSpaceReceiving() = default;
    // Exact API argument order; return preserves all32 EAX bits, not C++ bool.
    // Each explicit DWORD store is immediately visible through every alias.
    // Provider chooses store order, also on FALSE. Caller never copies back.
    virtual std::uint32_t get_disk_free_space_a(SourceMusicDriveRootAccess root,
                                                SourceMusicDiskDwordAccess sectors_per_cluster,
                                                SourceMusicDiskDwordAccess bytes_per_sector,
                                                SourceMusicDiskDwordAccess free_clusters,
                                                SourceMusicDiskDwordAccess total_clusters) = 0;
};

enum class SourceMusicDiskSpaceStop : std::uint8_t
{
    drive_return,
    bytes_per_sector_unknown,
    zero_bsf_unqualified,
    before_helper_45ffd0
};
struct SourceMusicDiskFreeSpacePrefixObservation
{
    SourceMusicDriveTypePrefixObservation drive;
    std::optional<std::uint32_t> api_result_bits;
    SourceMusicDiskWordState aliased_output;
    SourceMusicDiskWordState bytes_per_sector;
    SourceMusicDiskSpaceStop stop;
    std::optional<std::uint32_t> next_source_site;
    std::optional<std::uint8_t> written_shift;
    // Only BL can be bound after45FE53. No full EBX value is fabricated.
    static constexpr bool ebx_high24_bound = false;
    bool original_function_returned() const noexcept
    {
        return drive.original_function_returned();
    }
};

// One45FDA0 invocation and one root local through both external callbacks.
// Unknown/unwritten or zero BSF operand stops before45FE4B with no source-byte
// store. A supplied nonzero DWORD reaches the exact45FE56 byte store and stops
// before the unconditional45FE59 call45FFD0, regardless of the Win32 BOOL.
// No observation resumes a dead frame. No installed disk API is provided until
// actual initial/written native stack cells and undefined retail values qualify.
SourceMusicDiskFreeSpacePrefixObservation receive_source_music_disk_free_space_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform>, SourceMusicTimerCapsReceiving &, SourceMusicDriveTypeReceiving &,
    SourceMusicDiskFreeSpaceReceiving &);
} // namespace sgn::retail
