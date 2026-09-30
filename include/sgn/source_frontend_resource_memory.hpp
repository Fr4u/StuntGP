#pragma once
#include "sgn/source_boot_config.hpp"
#include "sgn/retail_frontend_language.hpp"
#include <optional>

namespace sgn::retail
{
namespace detail
{
struct FrontendResourceIdentity;
}
class SourceFrontendResourceMemory;
class SourceFrontendResourceRef final
{
    std::weak_ptr<const detail::FrontendResourceIdentity> owner_;
    std::uint32_t identity_{};
    friend class SourceFrontendResourceMemory;
    SourceFrontendResourceRef(std::weak_ptr<const detail::FrontendResourceIdentity>, std::uint32_t) noexcept;

  public:
    SourceFrontendResourceRef() = default;
    explicit operator bool() const noexcept
    {
        return identity_ != 0;
    }
    std::uint32_t identity() const noexcept
    {
        return identity_;
    }
    bool operator==(const SourceFrontendResourceRef &other) const noexcept
    {
        return identity_ == other.identity_ && !owner_.owner_before(other.owner_) && !other.owner_.owner_before(owner_);
    }
};
class SourceFrontendResourceError final : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceFrontendResourceError(std::uint32_t site, const char *why) : std::runtime_error(why), source_site(site) {}
};
// NEW receiving facts. The address is diagnostic, not permission to read bytes.
struct SourceFrontendResourceInfo
{
    std::uint32_t identity{}, payload_bytes{}, allocation_bytes{};
    std::uintptr_t native_address{};
    std::size_t initialized_prefix{};
    bool terminator_written{}, loader_returned{};
    std::optional<std::uint32_t> read_return;
};
enum class SourceFrontendResourceEventKind
{
    size_returned,
    allocated,
    read_returned,
    terminator_written,
    published,
    before_source_free,
    after_source_free,
    source_null,
    before_host_free,
    after_host_free
};
struct SourceFrontendResourceEvent
{
    SourceFrontendResourceEventKind kind;
    std::uint32_t source_site, source_identity;
    SourceFrontendResourceInfo resource;
};
struct SourceFrontendResourceObserver
{
    void *context{};
    // Synchronous non-owning host diagnostics; cannot throw or select branches.
    void (*observe)(void *, const SourceFrontendResourceEvent &) noexcept {};
};
struct SourceFrontendResourceAttempt
{
    SourceFrontendResourceRef resource;
    std::optional<std::uint32_t> size_return;
    bool loader_returned{}; // Includes the source null result for signed size<=0.
};
// One owner per actual frontend identity cell. Bound config/cell outlive it.
// It never owns the WAD cache or registers a42E8D0 bank allocation.
class SourceFrontendResourceMemory final
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    SourceFrontendResourceMemory(SourceBootConfigState &, std::uint32_t &source_identity,
                                 SourceFrontendResourceObserver observer = {});
    ~SourceFrontendResourceMemory();
    SourceFrontendResourceMemory(const SourceFrontendResourceMemory &) = delete;
    SourceFrontendResourceMemory &operator=(const SourceFrontendResourceMemory &) = delete;
    SourceFrontendResourceMemory(SourceFrontendResourceMemory &&) = delete;
    SourceFrontendResourceMemory &operator=(SourceFrontendResourceMemory &&) = delete;

    // Same config/root/cache, actual malloc(n+1), read(n), NUL[n]. No publication.
    // File platform is borrowed for this call only; raw allocation uses its
    // existing allocate_wad_raw primitive with the exact catalog request size.
    SourceFrontendResourceRef load_43c7f0(SourceBootFilePlatform &, std::string_view name);
    // Future440AE0 caller invokes this exactly at440B26, after its name writes.
    // Null is a source value. Neither null nor replacement frees a predecessor.
    void publish_440b26(const SourceFrontendResourceRef &);
    void dispose_440c20();
    // Temporary434B50 allocations only. Never changes/frees the published catalog.
    void release_462f9e(const SourceFrontendResourceRef &, std::uint32_t call_site);

    // No payload read: null/foreign/stale current words are diagnosed only when
    // a consumer resolves/reads them. The future parser supplies its actual PC.
    SourceFrontendResourceRef current_reference() const;
    SourceFrontendResourceAttempt last_attempt() const;
    SourceFrontendResourceInfo inspect(const SourceFrontendResourceRef &) const;
    std::size_t live_allocations() const;
    std::uint8_t read_byte(const SourceFrontendResourceRef &, std::uint32_t offset, std::uint32_t source_site) const;
    std::span<const std::uint8_t> initialized_prefix(const SourceFrontendResourceRef &) const;
    // Convenience for already fully initialized payloads. Partial input should
    // use read_byte; a separate NUL beyond a gap never initializes that gap.
    FrontendLanguageInput complete_input(const SourceFrontendResourceRef &, std::uint32_t source_site) const;
};
} // namespace sgn::retail
