#include "sgn/source_allocation_bank.hpp"
#include "sgn/host_profiler.hpp"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <utility>

namespace sgn::retail
{
namespace detail
{
struct SourceAllocationIdentity
{
};
} // namespace detail
namespace
{
// NEW R19 host identity source. Starts at 1 (0 is the null reference owner);
// 2^64 serials cannot be exhausted by bank construction.
std::atomic<std::uint64_t> next_bank_serial{1};
} // namespace
SourceAllocationBank::SourceAllocationBank() : serial_(next_bank_serial.fetch_add(1, std::memory_order_relaxed)) {}
SourceAllocationBank::~SourceAllocationBank()
{
    if (std::this_thread::get_id() != owner_thread_)
        std::terminate();
    // New host final cleanup AFTER consumer destruction. Not42EA00/42EAF0:
    // source may deliberately retain category1 or ordinals outside its window.
    for (const auto &record : records_)
        if (record.raw != 0)
            std::free(reinterpret_cast<void *>(record.raw));
}
void SourceAllocationBank::throw_wrong_thread()
{
    throw SourceAllocationError(0, "source allocation bank belongs to its creating thread");
}
void SourceAllocationBank::throw_stale_borrow()
{
    throw SourceAllocationError(0, "allocation borrow is null, foreign, expired or already released");
}
void SourceAllocationBank::throw_range(std::uint32_t site)
{
    throw SourceAllocationError(site, "source range outside actual allocation");
}
namespace
{
// Geometric growth for the host-only containers (an exact reserve would make
// repeated appends quadratic). Throws before any caller-visible mutation.
template <class Vector> void reserve_for(Vector &v, std::size_t needed)
{
    if (needed > v.capacity())
        v.reserve(std::max<std::size_t>(needed, std::max<std::size_t>(16, v.capacity() * 2)));
}
#if SGN_SOURCE_PROVENANCE
// First metadata entry whose byte offset is >= offset (map::lower_bound).
template <class Vector> auto opaque_lower(Vector &metadata, std::uint32_t offset)
{
    return std::lower_bound(metadata.begin(), metadata.end(), offset,
                            [](const auto &e, std::uint32_t at) { return e.offset < at; });
}
#endif
} // namespace
#if SGN_SOURCE_PROVENANCE
void SourceAllocationBank::check_known_slow(const OpaqueBytes &metadata, std::uint32_t offset, std::uint32_t length,
                                            std::uint32_t site)
{
    const auto first = opaque_lower(metadata, offset);
    if (first != metadata.end() && first->offset - offset < length)
        throw SourceOpaqueRead(site, first->byte.origin);
}
void SourceAllocationBank::erase_opaque(OpaqueBytes &metadata, std::uint32_t offset, std::uint32_t end) noexcept
{
    const auto first = opaque_lower(metadata, offset);
    const auto last =
        std::lower_bound(first, metadata.end(), end, [](const auto &e, std::uint32_t at) { return e.offset < at; });
    metadata.erase(first, last); // trivially copyable entries: no throw, no reallocation
}
#endif
void SourceAllocationBank::require_initialized() const
{
    check_thread();
    if (!initialized_)
        throw SourceAllocationError(0x42eab0, "source allocation bank requires explicit42EAB0 boot initialization");
}
std::size_t SourceAllocationBank::live_count() const
{
    check_thread();
    return static_cast<std::size_t>(
        std::count_if(records_.begin(), records_.end(), [](const auto &r) { return r.raw != 0; }));
}
std::size_t SourceAllocationBank::find_scan(const SourceAllocationRef &ref) const noexcept
{
    for (std::size_t i = 0; i < capacity; ++i)
        if (records_[i].aligned == ref.aligned_ && generations_[i] == ref.generation_)
            return i;
    return capacity;
}
SourceAllocationRef SourceAllocationBank::reference(std::size_t slot) const noexcept
{
    return SourceAllocationRef{serial_, generations_[slot], records_[slot].aligned, static_cast<std::uint32_t>(slot)};
}
void SourceAllocationBank::initialize_42eab0()
{
    check_thread();
    // Original blindly clears pointers without freeing them. Reinitializing
    // live host ownership is outside this cold-entry contract, not a free-all.
    if (live_count() != 0)
        throw SourceAllocationError(0x42eab0, "42EAB0 cannot discard registered live allocation ownership");
    records_ = {};
    generations_ = {};
    clear_opaque_all(); // 42EAB1..42EABD:384*24 source bytes.
    words_ = {};        // 42EABF..42EADD: seven source DWORDs, in zeroed final state.
    initialized_ = true;
    begin_42ea80(); // 42EAE3 tail, including42EA00 before active/ordinal writes.
}
void SourceAllocationBank::begin_42ea80()
{
    require_initialized();
    release_scope_42ea00(); // 42EA80; do not force-free category1 here.
    words_.active = 1;
    words_.next_ordinal = 0;
    words_.release_base = 0; // 42EA87/91/96.
    // NEW HOST (R30 perf-tick): a scene begins here (load, never inside a 60 Hz tick). Address tokens are
    // never recycled, so the token list and its hash index keep growing across scenes; their doubling
    // reallocation/rehash (O(all tokens ever issued)) used to land on whichever tick issued the next new
    // token. Reserve the scene's headroom now. Capacity only: no token, number or lookup result changes,
    // and a failed reservation is ignored (growth then happens on demand, as before).
    try
    {
        constexpr std::size_t headroom = std::size_t{1} << 16;
        const auto wanted = address_tokens_.size() + headroom;
        if (address_tokens_.capacity() < wanted)
            address_tokens_.reserve(std::max(wanted, address_tokens_.capacity() * 2));
        address_index_.reserve(wanted); // no-op when the bucket count already admits `wanted`
    }
    catch (...)
    {
    }
}
SourceAllocationRef SourceAllocationBank::allocate_42e8d0(std::uint32_t requested, std::uint32_t category)
{
    // 44EA8C ->42D660 ->42E8D0 precedes442143 ->42EAB0. The PE-zero
    //  records and counters already admit this allocation; do not begin/reset a scope.
    check_thread();
    std::size_t slot = 0;
    for (; slot < capacity; ++slot)
        if (records_[slot].raw == 0)
            break; // 42E8DB..42E8F2:+4 only.
    if (slot == capacity)
        return {}; // 42E8F4..42E8FB: no state writes or heap request.
    if (requested > std::numeric_limits<std::uint32_t>::max() - 0x24u)
        throw SourceAllocationError(0x42e8fd, "wrapped42E8D0 allocation size is outside the supported host domain");
    if (next_generation_ == std::numeric_limits<std::uint64_t>::max())
        throw SourceAllocationError(0, "source allocation host identity exhausted");
    const std::uint32_t reserved = requested + 0x24u;
    void *raw = std::malloc(static_cast<std::size_t>(reserved)); // 462FCD CRT boundary, actual payload allocation.
    if (!raw)
        throw SourceAllocationError(0x42e901,
                                    "CRT allocation failed; original unchecked null-memory writes are unsupported");
    const auto base = reinterpret_cast<std::uintptr_t>(raw);
    // Not ordinary round-up: an already32-aligned raw base advances by32.
    const auto aligned = (base + std::uintptr_t{0x20}) & ~std::uintptr_t{0x1f};           // 42E909/90C.
    records_[slot] = {aligned, base, reserved, requested, category, words_.next_ordinal}; // 42E912..925.
    clear_opaque(slot);
    generations_[slot] = next_generation_++;
    ++words_.next_ordinal; // 32-bit wrap,42E928..932.
    if (category == 1)
        words_.persistent_bytes += reserved;
    else
        words_.ordinary_bytes += reserved; // 42E938..942, exact-one category.
    const std::uint32_t sum = words_.ordinary_bytes + words_.persistent_bytes;
    if (sum > words_.peak_bytes)
        words_.peak_bytes = sum; // Unsigned42E95B..95F.
    std::memset(reinterpret_cast<void *>(aligned), 0,
                static_cast<std::size_t>(requested)); // 42E964..974, dword+byte tail.
    return reference(slot);                           // 42E978: same actual aligned allocation, no payload copy.
}
std::uint32_t SourceAllocationBank::release_42e980(const SourceAllocationRef &ref)
{
    check_thread();              // 42D6F4 frees the first CFG before actual42EAB0.
    const auto slot = find(ref); // 42E983..42E99C; null/unknown return0, no mutation.
    if (slot == capacity)
        return 0;
    auto &row = records_[slot];
    if (row.category == 1 && words_.release_persistent == 0)
        return row.reserved_bytes; // 42E9A3..9B8: retained, NOT freed.
    const auto reserved = row.reserved_bytes;
    if (row.category == 1)
        words_.persistent_bytes -= reserved;
    else
        words_.ordinary_bytes -= reserved;        // 42E9BA..9CA precedes external free.
    std::free(reinterpret_cast<void *>(row.raw)); // 42E9D0..9D4,462F9E CRT boundary.
    row = {};
    generations_[slot] = 0;
    clear_opaque(slot); // 42E9D9..9EC, all six source fields; host ref invalidation.
    return reserved;
}
void SourceAllocationBank::release_scope_42ea00()
{
    require_initialized();
    if (words_.active != 1)
        return; // Exact-one42EA06..0D.
    // Literal bounded window: NOT sorting all live allocation generations.
    // Persistent rows can share an ordinal with new rows after42EA80 resets it.
    for (std::uint32_t offset = static_cast<std::uint32_t>(capacity); offset != 0;)
    {
        --offset; // 42EA1F:383..0, including0 on the last complete scan.
        for (std::size_t slot = 0; slot < capacity; ++slot)
        {
            const auto &row = records_[slot];
            if (row.aligned == 0)
                continue; // 42EA25..2A:+0, distinct from allocator's+4.
            const std::uint32_t ordinal = words_.release_base + offset;
            if (row.ordinal != ordinal)
                continue; // 42EA2C..38, wrapping32-bit sum.
            if (row.category == 1 && words_.release_persistent == 0)
                continue;                          // 42EA3A..53: retained-byte local has no output.
            (void)release_42e980(reference(slot)); // 42EA55; ascending slot order within each ordinal.
        }
    }
    words_.active = 0; // 42EA6F, even when retained/out-of-window rows remain.
}
void SourceAllocationBank::release_42eaa0()
{
    release_scope_42ea00();
} // E9 5B FF FF FF.
void SourceAllocationBank::shutdown_42eaf0()
{
    require_initialized();
    words_.release_persistent = 1;
    release_scope_42ea00(); // No active=1 write.
}
#if SGN_SOURCE_PROVENANCE
SourceTransportWord SourceAllocationBank::load_opaque_word(std::size_t slot, std::uint32_t offset) const
{
    // Former per-byte map::find: metadata byte identity, else payload literal.
    const auto &metadata = opaque_bytes_[slot];
    const auto *bytes = payload(slot) + offset;
    SourceTransportWord result{};
    auto next = opaque_lower(metadata, offset);
    for (unsigned i = 0; i < 4; ++i)
    {
        if (next != metadata.end() && next->offset == offset + i)
        {
            result.bytes[i] = next->byte;
            ++next;
        }
        else
            result.bytes[i].literal = bytes[i];
    }
    return result;
}
void SourceAllocationBank::store_opaque_word(std::size_t slot, std::uint32_t offset, const SourceTransportWord &value)
{
    auto &metadata = opaque_bytes_[slot];
    const auto first = static_cast<std::size_t>(opaque_lower(metadata, offset) - metadata.begin());
    std::size_t last = first;
    while (last < metadata.size() && metadata[last].offset < offset + 4u)
        ++last;
    std::size_t incoming = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (value.bytes[i].origin)
            ++incoming;
    const auto replaced = last - first;
    // Any growth is reserved before the reached source store, like the former
    // prepared nodes: a failed host allocation leaves bytes and metadata intact.
    if (incoming > replaced)
        reserve_for(metadata, metadata.size() + (incoming - replaced));
    if (incoming > replaced)
    {
        const auto grow = incoming - replaced;
        const auto old = metadata.size();
        metadata.resize(old + grow); // capacity reserved: no reallocation/throw
        std::move_backward(metadata.begin() + static_cast<std::ptrdiff_t>(last),
                           metadata.begin() + static_cast<std::ptrdiff_t>(old), metadata.end());
    }
    else if (incoming < replaced)
    {
        metadata.erase(metadata.begin() + static_cast<std::ptrdiff_t>(first + incoming),
                       metadata.begin() + static_cast<std::ptrdiff_t>(last));
    }
    auto at = first;
    for (unsigned i = 0; i < 4; ++i)
        if (value.bytes[i].origin)
            metadata[at++] = {offset + i, value.bytes[i]};
    auto *bytes = payload(slot) + offset;
    for (unsigned i = 0; i < 4; ++i)
        if (!value.bytes[i].origin)
            bytes[i] = value.bytes[i].literal;
    // Unknown physical bytes are deliberately not assigned a fabricated value.
}
#endif
std::span<std::uint8_t> SourceAllocationBank::known_prefix(const SourceAllocationRef &ref, std::uint32_t offset,
                                                           std::uint32_t site) &
{
    const auto slot = checked_slot(ref, offset, 0, site);
#if SGN_SOURCE_PROVENANCE
    const auto &metadata = opaque_bytes_[slot];
    const auto next = opaque_lower(metadata, offset);
    const auto end = next == metadata.end() ? records_[slot].requested_bytes : next->offset;
#else
    const auto end = records_[slot].requested_bytes; // no opaque bytes in the shipped game
#endif
    return {payload(slot) + offset, end - offset};
}
std::uint32_t SourceAllocationBank::find_token(const SourceAddressLocation &location) const noexcept
{
    // Entries are unique under location equality (each is appended only when
    // absent), so this is the first and only match of the former linear scan.
    const auto found = address_index_.find(token_key(location));
    return found == address_index_.end() ? 0u : 0x80000000u + found->second;
}
std::uint32_t SourceAllocationBank::append_token(SourceAddressLocation location, std::uint32_t site)
{
    if (address_tokens_.size() >= 0x80000000ull)
        throw SourceAllocationError(site, "host address token domain exhausted");
    const auto index = static_cast<std::uint32_t>(address_tokens_.size());
    const auto key = token_key(location);
    // NEW HOST (R30): profiler marker for a mid-scene reallocation/rehash (should be rare after begin_42ea80).
    std::optional<sgn::host_profile::Scope> growth;
    if (address_tokens_.size() == address_tokens_.capacity() ||
        static_cast<float>(address_index_.size() + 1) >
            static_cast<float>(address_index_.bucket_count()) * address_index_.max_load_factor())
        growth.emplace("bank.token_growth");
    reserve_for(address_tokens_, address_tokens_.size() + 1); // may throw before any commit
    address_index_.emplace(key, index);                       // may throw; address_tokens_ is still unchanged
    address_tokens_.push_back(std::move(location));           // capacity reserved: cannot throw
    return 0x80000000u + index;
}
std::uint32_t SourceAllocationBank::encode_address(const SourceAllocationRef &allocation, std::uint32_t offset)
{
    require_initialized();
    if (!allocation)
    {
        if (offset)
            throw SourceAllocationError(0x4276c8, "NULL address with nonzero offset");
        return 0;
    }
    const auto payload_size = allocation_size(allocation);
    if (offset > payload_size)
        throw SourceAllocationError(0x4276c8, "address beyond allocation");
    if (const auto token = find_token({allocation, offset}))
        return token;
    return append_token({allocation, offset}, 0x4276c8);
}
SourceAddressLocation SourceAllocationBank::decode_address(std::uint32_t token) const
{
    require_initialized();
    if (!token)
        return {};
    if (token < 0x80000000u || static_cast<std::size_t>(token - 0x80000000u) >= address_tokens_.size())
        throw SourceAllocationError(0x4276c8, "unknown host address token");
    const auto &location = address_tokens_[token - 0x80000000u];
    const auto payload_size = allocation_size(location.allocation); // generation/thread checked, stale token rejected
    if (location.offset > payload_size)
        throw SourceAllocationError(0x4276c8, "token outside allocation");
    return location;
}
std::uint32_t SourceAllocationBank::advance_address(std::uint32_t token, std::uint32_t bytes)
{
    require_initialized();
    SourceAddressLocation location;
    if (token)
    {
        if (token < 0x80000000u || static_cast<std::size_t>(token - 0x80000000u) >= address_tokens_.size())
            throw SourceAllocationError(0x41cc5a, "unknown source address token at pointer publication");
        location = address_tokens_[token - 0x80000000u]; // no payload/lifetime/extent read
    }
    if (bytes > std::numeric_limits<std::uint32_t>::max() - location.offset)
        throw SourceAllocationError(0x41cc5a, "source pointer arithmetic leaves DWORD offset domain");
    location.offset += bytes;
    if (!location.allocation && location.offset == 0)
        return 0;
    if (const auto found = find_token(location))
        return found;
    return append_token(std::move(location), 0x41cc5a);
}

} // namespace sgn::retail
