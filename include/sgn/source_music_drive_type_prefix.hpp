#pragma once
#include "sgn/source_music_timer_entry_prefix.hpp"
#include <cstddef>
#include <optional>

namespace sgn::retail
{
class SourceMusicDriveTypeInstalled;
class SourceMusicDiskSpaceInstalled;

// Checked read-only borrow of this invocation's live five-byte root local.
// It exposes no native pointer, mutable storage, constructor or resume token.
// Escaped copies retain only borrow metadata; their platform owner is weak.
class SourceMusicDriveRootAccess final
{
    struct Scope;
    std::shared_ptr<Scope> scope_;
    explicit SourceMusicDriveRootAccess(std::shared_ptr<Scope>);
    std::shared_ptr<RaceSessionPlatform> checked_owner() const;
    const char *checked_native_root() const;
    friend struct SourceMusicPrefixExecution;
    friend class SourceMusicDriveTypeInstalled;
    friend class SourceMusicDiskSpaceInstalled;

  public:
    SourceMusicDriveRootAccess() = delete;
    SourceMusicDriveRootAccess(const SourceMusicDriveRootAccess &) = default;
    SourceMusicDriveRootAccess &operator=(const SourceMusicDriveRootAccess &) = default;
    ~SourceMusicDriveRootAccess() = default;
    static constexpr std::size_t byte_extent = 5;
    std::uint8_t byte(std::size_t offset) const;
};

class SourceMusicDriveTypeReceiving
{
  public:
    virtual ~SourceMusicDriveTypeReceiving() = default;
    virtual std::uint32_t get_drive_type_a(SourceMusicDriveRootAccess root) = 0;
};

// Diagnostic only. No subsequent music/file API accepts this as admission.
struct SourceMusicDriveTypePrefixObservation
{
    std::uint32_t minimum{}, maximum{}, selected_period{}, drive_type{};
    std::optional<std::uint32_t> source_return;
    std::optional<std::uint32_t> next_source_site;
    bool original_function_returned() const noexcept
    {
        return source_return.has_value();
    }
};

// Re-enters the original45FDA0 prologue, retains the same actual platform,
// requests its prepared period, copies its live filename and calls45FE17 once.
// Exactly result2 returns source bits80004005 at45FE2D. Every other uint32
// stops BEFORE45FE30. No result-state write, End, disk-space query or readiness.
SourceMusicDriveTypePrefixObservation receive_source_music_drive_type_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform>, SourceMusicTimerCapsReceiving &, SourceMusicDriveTypeReceiving &);

// Explicit installed entry. Uses SDK timeGetDevCaps and GetDriveTypeA directly;
// the persistent period owner must already have been prepared by its caller.
// This new entry is not called by the existing two-argument timer/root prefix.
SourceMusicDriveTypePrefixObservation receive_installed_source_music_drive_type_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform>);
} // namespace sgn::retail
