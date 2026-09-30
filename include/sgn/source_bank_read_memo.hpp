#pragma once
// NEW host performance helper (perf_world R19). Not recovered behaviour.
//
// Memoizes the checked admission of a few actual bank allocations for ONE
// synchronous source invocation (one producer/consumer call on the owner
// thread). It only uses the public bank API and never copies payload.
//
// Caller-proven contract for the memo lifetime:
//  - no bank allocation is allocated (42E8D0) or released (42E980/42EA00);
//  - no opaque transport word is stored (store_transport_word) into a
//    memoized allocation. write_known may occur: it only REMOVES opaque byte
//    metadata, so a memoized known prefix remains a valid lower bound, and
//    payload writes are observed because the memo holds the actual storage.
//
// Exactness: the first access of an allocation performs the same checks in
// the same order as the replaced accessor (thread, null/foreign/expired
// identity), so the first failure is unchanged. Afterwards a read inside the
// memoized opaque-free prefix returns the identical span the bank returns;
// any other read forwards to the exact bank accessor (its extent/opaque
// errors, sites and messages are therefore unchanged).
#include "sgn/source_allocation_bank.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

namespace sgn::retail
{
class SourceBankReadMemo
{
  public:
    explicit SourceBankReadMemo(SourceAllocationBank &bank) noexcept : bank_(bank) {}
    SourceBankReadMemo(const SourceBankReadMemo &) = delete;
    SourceBankReadMemo &operator=(const SourceBankReadMemo &) = delete;
    SourceAllocationBank &bank() const noexcept
    {
        return bank_;
    }
    // Same result and failures as bank.allocation_size(ref).
    std::uint32_t size(const SourceAllocationRef &ref)
    {
        return entry(ref).size;
    }
    // Same result and failures as bank.readable_bytes(ref,offset,length,site).
    std::span<std::uint8_t> readable(const SourceAllocationRef &ref, std::uint32_t offset, std::uint32_t length,
                                     std::uint32_t site)
    {
        const auto &e = entry(ref);
        if (offset <= e.known && length <= e.known - offset)
            return {e.data + offset, length};
        return bank_.readable_bytes(ref, offset, length, site);
    }
    // Same result and failures as bank.bytes(ref): the whole allocation, which
    // the bank rejects if ANY byte carries opaque transport metadata.
    std::span<std::uint8_t> whole(const SourceAllocationRef &ref)
    {
        const auto &e = entry(ref);
        if (e.known == e.size)
            return {e.data, e.size};
        return bank_.bytes(ref);
    }
    // bank.bytes(ref) followed by the caller's usual extent test: returns the
    // value, or throws SourceAllocationError(site,message) exactly like the
    // per-file helpers `read(bank,location,site)` this replaces.
    template <class T>
    T whole_read(const SourceAllocationRef &ref, std::uint32_t offset, std::uint32_t site, const char *message)
    {
        const auto bytes = whole(ref);
        if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
            throw SourceAllocationError(site, message);
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }

  private:
    struct Entry
    {
        SourceAllocationRef ref;
        std::uint8_t *data{};
        std::uint32_t known{}, size{};
    };
    static constexpr unsigned capacity = 16;
    SourceAllocationBank &bank_;
    std::array<Entry, capacity> entries_{};
    unsigned used_{};
    // The returned reference is used before the next entry() call.
    const Entry &entry(const SourceAllocationRef &ref)
    {
        for (unsigned i = 0; i < used_; ++i)
        {
            if (!(entries_[i].ref == ref))
                continue;
            if (i == 0)
                return entries_[0];
            std::swap(entries_[i], entries_[i - 1]); // transpose: hot allocations drift to the front
            return entries_[i - 1];
        }
        // Miss: thread and identity admission identical to allocation_size /
        // checked_slot (same exception type, site0 and message).
        const auto size = bank_.allocation_size(ref);
        const auto prefix = bank_.known_prefix(ref, 0, 0); // contiguous non-opaque prefix
        auto &e = used_ < capacity ? entries_[used_++] : entries_[capacity - 1];
        e.ref = ref;
        e.data = prefix.data();
        e.known = static_cast<std::uint32_t>(prefix.size());
        e.size = size;
        return e;
    }
};
} // namespace sgn::retail
