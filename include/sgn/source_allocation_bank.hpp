#pragma once
#include "sgn/source_transport_word.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map> //kept for includers that relied on the former std::map metadata include
#include <memory>
#include <span>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgn::retail
{
namespace detail
{
struct SourceAllocationIdentity;
}
class SourceAllocationBank;

class SourceAllocationError final : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceAllocationError(std::uint32_t site, const char *message) : std::runtime_error(message), source_site(site) {}
};

// Host identity for one actual allocation, not a truncated x86/host address.
// The identity does not retain memory or extend the bank's lifetime.
// NEW R19 (perf_transport): the owning bank is identified by a process-unique,
// never reused 64-bit bank serial (0 = no bank) instead of a weak_ptr control
// block. Equality classes are unchanged (same bank, generation and aligned
// payload; a destroyed bank's references never equal a later bank's), but a
// reference is now trivially copyable: copies touch no atomic counters.
class SourceAllocationRef final
{
    std::uint64_t owner_{};
    std::uint64_t generation_{};
    std::uintptr_t aligned_{};
    // NEW host lookup hint (not part of identity/equality): the slot that
    // produced this reference. SourceAllocationBank::find verifies it against
    // the live aligned pointer and generation before use.
    std::uint32_t slot_hint_{0xffffffffu};
    friend class SourceAllocationBank;
    constexpr SourceAllocationRef(std::uint64_t owner, std::uint64_t generation, std::uintptr_t aligned,
                                  std::uint32_t slot) noexcept
        : owner_(owner), generation_(generation), aligned_(aligned), slot_hint_(slot)
    {
    }

  public:
    SourceAllocationRef() = default; // Original null pointer, including a full bank.
    explicit operator bool() const noexcept
    {
        return aligned_ != 0;
    }
    // Safe after release/bank destruction: compares identities only, without
    // dereferencing the bank or either payload.
    bool operator==(const SourceAllocationRef &other) const noexcept
    {
        return aligned_ == other.aligned_ && generation_ == other.generation_ && owner_ == other.owner_;
    }
};
static_assert(std::is_trivially_copyable_v<SourceAllocationRef>);

// New host-only pointer transport. Tokens are never guest numeric addresses.
struct SourceAddressLocation
{
    SourceAllocationRef allocation;
    std::uint32_t offset{};
};
// The seven source DWORDs remain distinct. Arithmetic wraps at32 bits.
struct SourceAllocationWords
{
    std::uint32_t active{};             // 589E10
    std::uint32_t ordinary_bytes{};     // 589E14; category !=1
    std::uint32_t persistent_bytes{};   // 589E18; category ==1
    std::uint32_t peak_bytes{};         // 589E1C; unsigned maximum of wrapped sum
    std::uint32_t next_ordinal{};       // 589E20
    std::uint32_t release_base{};       // 589E24
    std::uint32_t release_persistent{}; // 589E28; zero inhibits category1 free
};

// Source24-byte row at589E2C+i*24, with actual native-width pointers here.
// Do not reinterpret this struct as an x86 bank or narrow its pointers.
struct SourceAllocationRecord
{
    std::uintptr_t aligned{}, raw{}; // source+0/+4
    std::uint32_t reserved_bytes{};  // source+8 = requested+0x24
    std::uint32_t requested_bytes{}; // source+0C
    std::uint32_t category{};        // source+10; only exact1 is persistent
    std::uint32_t ordinal{};         // source+14
};

// Shared by actual producer calls across levels, owned once by the platform.
// Constructor only represents PE-zero storage; it does NOT call42EAB0.
// 44EA8C reaches42D660 allocation/free before442143 calls42EAB0. Those
// operations are valid on the PE-zero bank; initialized() tracks42EAB0 only.
// Buffers belong solely to this bank. All access/destruction is owner-thread.
//
// NEW R19 (perf_transport): host representation only. The hot borrow paths
// (lookup, extent, readable/known/transport access) are inline here; address
// tokens keep their issue order and numbering and gain a hash index for
// encode/advance. Lifetime, thread and extent checks, their order and sites
// are unchanged.
// Shipped game (SGN_SOURCE_PROVENANCE=0, ROOT DIRECTIVE): plain byte storage,
// no per-byte opaque metadata; every source word is known (see
// source_transport_word.hpp for the documented 0 default of unproduced words).
// Diagnostic build (=1): opaque byte metadata is one flat vector per slot,
// sorted by byte offset, and numeric borrows of opaque bytes throw
// SourceOpaqueRead at the borrowing site.
class SourceAllocationBank final
{
  public:
    static constexpr std::size_t capacity = 384;

  private:
    struct OpaqueEntry
    {
        std::uint32_t offset;
        SourceTransportByte byte;
    };
    using OpaqueBytes = std::vector<OpaqueEntry>; // strictly ascending offsets
    // Exactly the SourceAddressLocation equality (allocation operator== plus offset).
    struct TokenKey
    {
        std::uint64_t owner, generation;
        std::uintptr_t aligned;
        std::uint32_t offset;
        bool operator==(const TokenKey &) const = default;
    };
    struct TokenHash
    {
        std::size_t operator()(const TokenKey &k) const noexcept
        {
            std::uint64_t h = static_cast<std::uint64_t>(k.aligned) * 0x9e3779b97f4a7c15ull;
            h ^= (k.generation + 0x632be59bd9b4e019ull + (h << 6) + (h >> 2));
            h ^= (std::uint64_t(k.offset) * 0xbf58476d1ce4e5b9ull) + (h << 6) + (h >> 2);
            h ^= (k.owner * 0x94d049bb133111ebull) + (h << 6) + (h >> 2);
            return static_cast<std::size_t>(h ^ (h >> 31));
        }
    };
    static TokenKey token_key(const SourceAddressLocation &l) noexcept
    {
        return {l.allocation.owner_, l.allocation.generation_, l.allocation.aligned_, l.offset};
    }
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    const std::uint64_t serial_; // process-unique, never reused (NEW R19 identity)
    std::array<SourceAllocationRecord, capacity> records_{};
    std::array<std::uint64_t, capacity> generations_{}; // Host validity, not source ordinal.
#if SGN_SOURCE_PROVENANCE
    std::array<OpaqueBytes, capacity> opaque_bytes_;
#endif
    SourceAllocationWords words_{};
    std::uint64_t next_generation_{1}; // Never reset by a source initializer.
    // No payload: monotonically issued identity/offset metadata. Never recycle
    // on level release or bank initialization: stale packets must stay stale.
    std::vector<SourceAddressLocation> address_tokens_;
    // NEW host index over address_tokens_ (same entries and numbering, never
    // erased). Tokens are unique under location equality, so the index returns
    // exactly the entry the former first-match linear scan returned.
    std::unordered_map<TokenKey, std::uint32_t, TokenHash> address_index_;
    bool initialized_{}; // Observation of actual42EAB0, not raw allocation readiness.

    [[noreturn]] static void throw_wrong_thread();
    [[noreturn]] static void throw_stale_borrow();
    [[noreturn]] static void throw_range(std::uint32_t site);
    void check_thread() const
    {
        if (std::this_thread::get_id() != owner_thread_)
            throw_wrong_thread();
    }
    void require_initialized() const;
    std::size_t find(const SourceAllocationRef &ref) const noexcept
    {
        // Same bank (serial), then the verified slot hint, then the full scan.
        if (!ref || ref.owner_ != serial_)
            return capacity;
        const auto hint = ref.slot_hint_;
        if (hint < capacity && records_[hint].aligned == ref.aligned_ && generations_[hint] == ref.generation_)
            return hint;
        return find_scan(ref);
    }
    std::size_t find_scan(const SourceAllocationRef &) const noexcept;
    // Same checks and error order as allocation_size, one lookup.
    std::size_t live_slot(const SourceAllocationRef &ref) const
    {
        check_thread();
        const auto slot = find(ref);
        if (slot == capacity)
            throw_stale_borrow();
        return slot;
    }
    // Same checks and error order as allocation_size + check_extent, one lookup.
    std::size_t checked_slot(const SourceAllocationRef &ref, std::uint32_t offset, std::uint32_t length,
                             std::uint32_t site) const
    {
        const auto slot = live_slot(ref);
        const auto size = records_[slot].requested_bytes;
        if (offset > size || length > size - offset)
            throw_range(site);
        return slot;
    }
    // First opaque byte in [offset,offset+length) is diagnosed at site.
    void check_known(std::size_t slot, std::uint32_t offset, std::uint32_t length, std::uint32_t site) const
    {
#if SGN_SOURCE_PROVENANCE
        const auto &metadata = opaque_bytes_[slot];
        if (!metadata.empty())
            check_known_slow(metadata, offset, length, site);
#else
        (void)slot;
        (void)offset;
        (void)length;
        (void)site;
#endif
    }
#if SGN_SOURCE_PROVENANCE
    static void check_known_slow(const OpaqueBytes &, std::uint32_t offset, std::uint32_t length, std::uint32_t site);
    static void erase_opaque(OpaqueBytes &, std::uint32_t offset, std::uint32_t end) noexcept;
    SourceTransportWord load_opaque_word(std::size_t slot, std::uint32_t offset) const;
    void store_opaque_word(std::size_t slot, std::uint32_t offset, const SourceTransportWord &);
    void clear_opaque(std::size_t slot) noexcept
    {
        opaque_bytes_[slot].clear();
    }
    void clear_opaque_all() noexcept
    {
        for (auto &metadata : opaque_bytes_)
            metadata.clear();
    }
#else
    void clear_opaque(std::size_t) noexcept {}
    void clear_opaque_all() noexcept {}
#endif
    std::uint8_t *payload(std::size_t slot) const noexcept
    {
        return reinterpret_cast<std::uint8_t *>(records_[slot].aligned);
    }
    SourceAllocationRef reference(std::size_t) const noexcept;
    std::uint32_t find_token(const SourceAddressLocation &) const noexcept; // 0 when absent
    std::uint32_t append_token(SourceAddressLocation, std::uint32_t site);

  public:
    SourceAllocationBank();
    ~SourceAllocationBank();
    SourceAllocationBank(const SourceAllocationBank &) = delete;
    SourceAllocationBank &operator=(const SourceAllocationBank &) = delete;
    SourceAllocationBank(SourceAllocationBank &&) = delete;
    SourceAllocationBank &operator=(SourceAllocationBank &&) = delete;
    // Zero denotes NULL. Nonzero tokens cannot be added/subtracted as addresses.
    std::uint32_t encode_address(const SourceAllocationRef &, std::uint32_t offset);
    SourceAddressLocation decode_address(std::uint32_t token) const;
    // Metadata-only pointer arithmetic. Keeps captured allocation/generation,
    // including released/NULL identities; payload admission stays at decode.
    std::uint32_t advance_address(std::uint32_t token, std::uint32_t bytes);
    bool initialized() const
    {
        check_thread();
        return initialized_;
    }
    const SourceAllocationWords &source_words() const &
    {
        check_thread();
        return words_;
    }
    const SourceAllocationWords &source_words() const && = delete;
    std::span<const SourceAllocationRecord, capacity> source_records() const &
    {
        check_thread();
        return records_;
    }
    std::span<const SourceAllocationRecord, capacity> source_records() const && = delete;
    std::size_t live_count() const;
    void initialize_42eab0(); // Clear bank+7words, then actual42EA80; rejects live ownership.
    void begin_42ea80();      // Release old active scope, active=1, ordinal=base=0.
    SourceAllocationRef allocate_42e8d0(std::uint32_t requested_bytes, std::uint32_t category);
    std::uint32_t release_42e980(const SourceAllocationRef &);
    void release_scope_42ea00(); // Exact bounded reverse-ordinal/ascending-slot traversal.
    void release_42eaa0();       // Literal tail42EA00.
    void shutdown_42eaf0();      // release_persistent=1 then42EA00; never forces active=1.
    // NEW metadata admission never interprets payload. Diagnostic build: all
    // numeric borrows reject opaque bytes; known writes clear ONLY their range.
    std::uint32_t allocation_size(const SourceAllocationRef &ref) const
    {
        return records_[live_slot(ref)].requested_bytes;
    }
    void check_extent(const SourceAllocationRef &ref, std::uint32_t offset, std::uint32_t length,
                      std::uint32_t site = 0) const
    {
        (void)checked_slot(ref, offset, length, site);
    }
    std::span<std::uint8_t> readable_bytes(const SourceAllocationRef &ref, std::uint32_t offset, std::uint32_t length,
                                           std::uint32_t site = 0) &
    {
        const auto slot = checked_slot(ref, offset, length, site);
        check_known(slot, offset, length, site);
        return {payload(slot) + offset, length};
    }
    std::span<const std::uint8_t> readable_bytes(const SourceAllocationRef &ref, std::uint32_t offset,
                                                 std::uint32_t length, std::uint32_t site = 0) const &
    {
        const auto slot = checked_slot(ref, offset, length, site);
        check_known(slot, offset, length, site);
        return {payload(slot) + offset, length};
    }
    std::span<std::uint8_t> readable_bytes(const SourceAllocationRef &, std::uint32_t, std::uint32_t,
                                           std::uint32_t = 0) && = delete;
    std::span<const std::uint8_t> readable_bytes(const SourceAllocationRef &, std::uint32_t, std::uint32_t,
                                                 std::uint32_t = 0) const && = delete;
    void write_known(const SourceAllocationRef &ref, std::uint32_t offset, std::span<const std::uint8_t> value,
                     std::uint32_t site = 0)
    {
        if (value.size() > 0xffffffffull)
            throw SourceAllocationError(site, "known source write exceeds DWORD extent");
        const auto size = static_cast<std::uint32_t>(value.size());
        const auto slot = checked_slot(ref, offset, size, site);
        // memmove admits same-allocation overlap without a payload mirror.
        std::memmove(payload(slot) + offset, value.data(), size);
#if SGN_SOURCE_PROVENANCE
        auto &metadata = opaque_bytes_[slot];
        if (!metadata.empty())
            erase_opaque(metadata, offset, offset + size);
#endif
    }
    SourceTransportWord load_transport_word(const SourceAllocationRef &ref, std::uint32_t offset,
                                            std::uint32_t site) const
    {
        const auto slot = checked_slot(ref, offset, 4, site);
#if SGN_SOURCE_PROVENANCE
        if (!opaque_bytes_[slot].empty())
            return load_opaque_word(slot, offset);
#endif
        std::uint32_t value;
        std::memcpy(&value, payload(slot) + offset, 4);
        return SourceTransportWord::known(value);
    }
    void store_transport_word(const SourceAllocationRef &ref, std::uint32_t offset, const SourceTransportWord &value,
                              std::uint32_t site)
    {
        const auto slot = checked_slot(ref, offset, 4, site);
#if SGN_SOURCE_PROVENANCE
        if (!opaque_bytes_[slot].empty() || !value.is_known())
        {
            store_opaque_word(slot, offset, value);
            return;
        }
#endif
        auto *bytes = payload(slot) + offset;
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = value.bytes[i].literal;
    }
    // A synchronous typed tail may expose only its contiguous known prefix.
    // It must not be retained across any source call/store/release.
    std::span<std::uint8_t> known_prefix(const SourceAllocationRef &, std::uint32_t offset, std::uint32_t site) &;
    std::span<std::uint8_t> known_prefix(const SourceAllocationRef &, std::uint32_t, std::uint32_t) && = delete;
    // Actual zero-filled payload; synchronous borrow only, invalidated by
    // release or bank destruction. No second vector/copy or retained owner.
    std::span<std::uint8_t> bytes(const SourceAllocationRef &ref) &
    {
        const auto slot = live_slot(ref);
        const auto size = records_[slot].requested_bytes;
        check_known(slot, 0, size, 0);
        return {payload(slot), size};
    }
    std::span<const std::uint8_t> bytes(const SourceAllocationRef &ref) const &
    {
        const auto slot = live_slot(ref);
        const auto size = records_[slot].requested_bytes;
        check_known(slot, 0, size, 0);
        return {payload(slot), size};
    }
    std::span<std::uint8_t> bytes(const SourceAllocationRef &) && = delete;
    std::span<const std::uint8_t> bytes(const SourceAllocationRef &) const && = delete;
};
} // namespace sgn::retail
