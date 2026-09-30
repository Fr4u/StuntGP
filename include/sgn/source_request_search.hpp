#pragma once
#include "sgn/source_pmd_requests.hpp"
#include "sgn/source_application_literals.hpp"

namespace sgn::retail
{
enum class SourceSearchReadOwner
{
    haystack,
    needle
};
struct SourceSearchRead
{
    SourceSearchReadOwner owner;
    std::uint32_t site, address, width, value, ordinal;
    bool operator==(const SourceSearchRead &) const = default;
};
struct SourceSearchReadObserver
{
    virtual ~SourceSearchReadObserver() = default;
    // NEW diagnostic observation after a completed logical source read. The
    // callback supplies no data; actual owners remain the sole read source.
    // Throwing stops at that reached prefix. Explicit diagnostic mutations
    // through real owners are not retail writes or concurrent memory access.
    virtual void after_read(const SourceSearchRead &) = 0;
};
// 462890, including its jump into464F66 and all shared returns. Bounded
//  mapping for H in the actual768-byte request region and N in the actual
//  application literal region. No fullPE/stack alias or other callsite claim.
//  No writes, source continuation after RET, copied strings or owning result.
//  Empty N returns original H, including null/expired, without reading H.
SourceRequestName find_source_request_462890(SourceRequestName haystack, const SourceApplicationLiteralRef &needle,
                                             std::uint32_t needle_address,
                                             SourceSearchReadObserver *observer = nullptr);
} // namespace sgn::retail
