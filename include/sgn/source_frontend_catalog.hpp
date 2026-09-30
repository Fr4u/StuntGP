#pragma once
#include "sgn/retail_frontend_state.hpp"
#include <stdexcept>

namespace sgn::retail
{
// Explicit host frontiers for undefined local bytes/values and catalog bounds.
// External input read failures retain SourceFrontendResourceError and its PC.
class SourceFrontendCatalogError final : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceFrontendCatalogError(std::uint32_t site, const char *why) : std::runtime_error(why), source_site(site) {}
};
enum class SourceFrontendCatalogEventKind
{
    before_read,
    after_store
};
struct SourceFrontendCatalogEvent
{
    SourceFrontendCatalogEventKind kind;
    std::uint32_t source_site;
    std::uint32_t source_address; // after_store only; original virtual address
    std::uint32_t width;          // after_store only,1 or4 bytes
    std::uint32_t value;          // normalized identities, never truncated host pointers
    std::uint32_t input_identity; // before_read only
    std::uint32_t input_offset;   // before_read only, observed before resolving
};
struct SourceFrontendCatalogObserver
{
    void *context{};
    // Synchronous, readonly diagnostics. Must not mutate/reenter any owner.
    void (*observe)(void *, const SourceFrontendCatalogEvent &) noexcept {};
};
namespace detail
{
// Borrowed implementation boundary. Precondition: memory is bound to this
// storage's identity cell and the same world boot config. No owners are copied.
// The production FrontendState entry below establishes that binding.
std::uint32_t source_frontend_catalog_440ae0(FrontendSourceStorage &, SourceFrontendResourceMemory &,
                                             SourceBootFilePlatform &, SourceFrontendCatalogObserver);
} // namespace detail
// Actual440AE0 child, with source load/read/store order and no preflight/rollback.
// Cursor is a host diagnostic; original440AE0 returns void. This entry does not
// advance the outer43D0B0 parent or activate any later frontend child.
inline std::uint32_t initialize_source_frontend_catalog_440ae0(FrontendState &frontend, SourceBootFilePlatform &files,
                                                               SourceFrontendCatalogObserver observer = {})
{
    return detail::source_frontend_catalog_440ae0(frontend.source, frontend.resource_memory(), files, observer);
}
} // namespace sgn::retail
