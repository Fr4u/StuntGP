#pragma once
// New private diagnostic wire policy, not recovered retail game behavior.
#include <cstddef>
#include <cstdint>
#include <limits>

namespace normal_session_owner::wire_limits {
inline constexpr std::uint32_t snapshots=1024;
inline constexpr std::uint32_t legacy_allocations_per_snapshot=4096;
inline constexpr std::uint32_t v3_allocations_per_snapshot=16384;
inline constexpr std::uint64_t v3_allocations_per_file=65536;
inline constexpr std::uint64_t allocation_bytes=64ull*1024*1024;
inline constexpr std::uint64_t payload_bytes_per_file=256ull*1024*1024;
inline constexpr std::uint32_t aliases_per_snapshot=262144;
inline constexpr std::uint64_t v3_aliases_per_file=262144;
inline constexpr std::uint32_t claims_per_owner=65536;
inline constexpr std::uint64_t v3_claims_per_file=1048576;
inline constexpr std::size_t label_bytes=512;
inline constexpr std::uint64_t v3_metadata_bytes_per_file=64ull*1024*1024;
inline constexpr std::uint64_t v3_wire_bytes_per_file=320ull*1024*1024;
inline constexpr std::uint64_t address_space_end=0x100000000ull;
inline constexpr std::uint64_t unlimited=std::numeric_limits<std::uint64_t>::max();

struct Policy {
    std::uint32_t allocations_per_snapshot;
    std::uint64_t allocations_per_file,aliases_per_file,claims_per_file,metadata_bytes,wire_bytes;
};
// Callers validate version in [1,3] before choosing a policy. Legacy aggregate
// allocation/alias bounds below are just their existing per-snapshot bounds
// times the existing snapshot bound. V3's new metadata limits do not affect v1/2.
inline constexpr Policy policy(unsigned version) noexcept {
    if(version==3)return {v3_allocations_per_snapshot,v3_allocations_per_file,
        v3_aliases_per_file,v3_claims_per_file,v3_metadata_bytes_per_file,v3_wire_bytes_per_file};
    return {legacy_allocations_per_snapshot,std::uint64_t(legacy_allocations_per_snapshot)*snapshots,
        std::uint64_t(aliases_per_snapshot)*snapshots,unlimited,unlimited,unlimited};
}
}
