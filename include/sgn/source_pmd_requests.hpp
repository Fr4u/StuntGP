#pragma once
#include "sgn/retail_level.hpp"
#include "sgn/source_462890_address.hpp"
#include <bit>
#include <cstring>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>

namespace sgn::retail
{
class ModelResources;
class SourcePmdRequests;
namespace detail
{
struct SourceRequestIdentity
{
    const SourcePmdRequests *owner;
    const std::thread::id thread;
};
} // namespace detail
class SourceRequestError final : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceRequestError(std::uint32_t site, const char *message) : std::runtime_error(message), source_site(site) {}
};
// NEW weak identity for the sole static768-byte source region, not a bank
// allocation or truncated host address. Non-null does not imply still alive.
class SourceRequestName final
{
    std::weak_ptr<const detail::SourceRequestIdentity> owner_;
    std::uint32_t offset_{};
    bool nonnull_{};
    friend class SourcePmdRequests;
    SourceRequestName(const std::shared_ptr<const detail::SourceRequestIdentity> &owner, std::uint32_t offset)
        : owner_(owner), offset_(offset), nonnull_(true)
    {
    }
    std::shared_ptr<const detail::SourceRequestIdentity> checked(std::uint32_t site) const;

  public:
    SourceRequestName() = default;
    explicit operator bool() const noexcept
    {
        return nonnull_;
    }
    // Pointer-value inspection/equality remains valid after owner expiry.
    std::uint32_t offset() const noexcept
    {
        return offset_;
    }
    bool operator==(const SourceRequestName &other) const noexcept
    {
        return nonnull_ == other.nonnull_ && offset_ == other.offset_ && !owner_.owner_before(other.owner_) &&
               !other.owner_.owner_before(owner_);
    }
    // 462890 register-value/alias APIs never dereference or retain the owner.
    std::uint32_t source_address_value_462890() const noexcept
    {
        return nonnull_ ? source_462890::add(0x58c238, offset_) : 0;
    }
    SourceRequestName alias_value_462890(std::uint32_t source_address) const;
    // Reached memory operations only; arithmetic uses source_462890 values.
    std::uint8_t read_u8_462890(std::uint32_t source_address, std::uint32_t source_site) const;
    std::uint32_t read_u32le_462890(std::uint32_t source_address, std::uint32_t source_site) const;
    char byte(std::uint32_t relative = 0) const;
    std::uint8_t read_formatter_byte(std::uint32_t relative, std::uint32_t site) const;
    SourceRequestName advanced(std::uint32_t bytes) const;
    std::string text_snapshot() const; // diagnostic value copy, never a source owner
};
struct SourceRequestRecord
{
    SourceRequestName name; // source58C540+i*8, not native sizeof(record)
    std::uint32_t role{};   // source+4, preserved for a newly written sentinel
    bool operator==(const SourceRequestRecord &) const = default;
};
struct SourceRequestValues
{
    std::array<char, 768> names;
    std::array<SourceRequestRecord, 24> records;
    SourceRequestName append, track_name;
    std::uint32_t count;
    bool operator==(const SourceRequestValues &) const = default;
};
struct SourceRequestStore
{
    std::uint32_t site, target, bytes, ordinal;
};
struct SourceRequestObserver
{
    virtual ~SourceRequestObserver() = default;
    // Read-only callback after one logical persistent source store. Throwing
    // retains the exact reached prefix. The actual owner must outlive the call.
    virtual void after_store(const SourceRequestStore &, const SourcePmdRequests &) = 0;
};
struct SourceRequestBoundary
{
    std::uint32_t next_site, required_callee;
    bool reached_return;
    std::uint32_t script_cursor{}; // borrowed script offset at the explicit boundary
};
class SourcePmdRequests final
{
    const std::shared_ptr<const detail::SourceRequestIdentity> identity_;
    std::array<char, 768> names_{};
    std::array<SourceRequestRecord, 24> records_{};
    SourceRequestName append_;     // 58C234, null until actual42D10C publication
    std::uint32_t count_{};        // 58C808
    SourceRequestName track_name_; // 624460 aliases names_; not reset by42EB00
    friend class SourceRequestName;
    friend class ModelResources;
    friend class SceneResources;         // 42F090 consumes this same live request region
    friend struct SourcePmdLoadingState; // same owner completes the42EB00 reset
    void check_thread(std::uint32_t site) const
    {
        if (std::this_thread::get_id() != identity_->thread)
            throw SourceRequestError(site, "source request owner belongs to another thread");
    }
    static void require(bool condition, std::uint32_t site, const char *message)
    {
        if (!condition)
            throw SourceRequestError(site, message);
    }
    void event(SourceRequestObserver *observer, std::uint32_t &ordinal, std::uint32_t site, std::uint32_t target,
               std::uint32_t bytes) const
    {
        ++ordinal;
        if (observer)
            observer->after_store({site, target, bytes, ordinal}, *this);
    }
    static void validate(LevelBlock block)
    {
        // NEW admitted domain: a real non-null source key-search boundary,
        // complete bounded script bytes, and no embedded C-string terminator.
        require(block.end > 0 && block.end <= block.text.size() &&
                    block.text.size() <= std::numeric_limits<std::uint32_t>::max(),
                0x42c580, "source key-search boundary is outside the bounded script");
        require(block.text.find('\0') == std::string_view::npos, 0x42c580,
                "embedded script terminator outside admitted domain");
    }
    static bool key_42c580(LevelBlock block, std::string_view key, std::size_t &cursor)
    {
        const auto found = block.text.find(key, cursor); // 462890 case-sensitive substring
        if (found == std::string_view::npos || found > block.end)
            return false;
        const auto equal = block.text.find('=', found); // 4777F0 literal
        require(equal != std::string_view::npos, 0x42c5b2, "key has no source equals delimiter");
        const auto space = block.text.find(' ', equal); // 42C5BE..42C5CC, literal20 only
        require(space != std::string_view::npos, 0x42c5be, "key has no following literal space");
        cursor = space;
        return true;
    }
    static bool delimiter(char value) noexcept
    {
        return value == ' ' || value == '\r' || value == '\n';
    }
    void token_42c5e0(LevelBlock block, std::size_t &cursor, SourceRequestObserver *observer, std::uint32_t &ordinal,
                      std::uint32_t call_site)
    {
        require(static_cast<bool>(append_), call_site, "source append pointer is null");
        require(count_ < records_.size() - 1, call_site, "request has no in-region sentinel slot");
        std::size_t begin = cursor;
        while (begin < block.text.size() && delimiter(block.text[begin]))
            ++begin;
        require(begin < block.text.size(), 0x42c5eb, "unterminated source token prefix");
        auto end = begin;
        while (end < block.text.size() && !delimiter(block.text[end]))
            ++end;
        require(end < block.text.size(), 0x42c614, "source token lacks its terminating delimiter");
        const auto length = end - begin;
        const auto offset = append_.offset_;
        require(offset < names_.size() && length < names_.size() - offset, 0x42c61b,
                "source name exceeds the sole768-byte region");
        // Preflight excludes invalid source accesses; admitted writes follow
        // 42C61B byte order and42C62D terminator. No token/string owner is made.
        for (std::size_t i = 0; i < length; ++i)
        {
            names_[offset + i] = block.text[begin + i];
            event(observer, ordinal, 0x42c61b, 0x58c238 + offset + static_cast<std::uint32_t>(i), 1);
        }
        names_[offset + length] = 0;
        event(observer, ordinal, 0x42c62d, 0x58c238 + offset + static_cast<std::uint32_t>(length), 1);
        cursor = end + 1; // 42C638: after delimiter, not the first byte of next token
    }
    std::uint32_t name_extent(std::uint32_t site) const
    {
        const auto first = append_.offset_;
        auto end = first;
        while (end < names_.size() && names_[end] != 0)
            ++end;
        require(end < names_.size(), site, "source name lost its terminator");
        return end - first + 1;
    }
    SourceRequestBoundary initialize_42d070_prefix(SourceRequestObserver *observer)
    {
        check_thread(0x42d10c);
        std::uint32_t ordinal = 0;
        append_ = SourceRequestName(identity_, 0);
        event(observer, ordinal, 0x42d10c, 0x58c234, 4);
        records_[0].name = {};
        event(observer, ordinal, 0x42d116, 0x58c540, 4);
        count_ = 0;
        event(observer, ordinal, 0x42d11c, 0x58c808, 4);
        return {0x42d122, 0x42c740, false};
    }
    SourceRequestBoundary produce_42c740_prefix(LevelBlock block, SourceRequestObserver *observer)
    {
        check_thread(0x42c740);
        validate(block);
        std::size_t cursor = 0;
        std::uint32_t ordinal = 0;
        if (key_42c580(block, "L_SKY", cursor))
        {
            token_42c5e0(block, cursor, observer, ordinal, 0x42c76b);
            const auto name = append_;
            const auto extent = name_extent(0x42c783);
            const auto index = count_;
            records_[index].name = name;
            event(observer, ordinal, 0x42c787, 0x58c540 + index * 8, 4);
            records_[index].role = 2;
            event(observer, ordinal, 0x42c78f, 0x58c544 + index * 8, 4);
            count_ = index + 1;
            event(observer, ordinal, 0x42c79f, 0x58c808, 4);
            append_ = SourceRequestName(identity_, name.offset_ + extent);
            event(observer, ordinal, 0x42c7a5, 0x58c234, 4);
            records_[count_].name = {};
            event(observer, ordinal, 0x42c7ab, 0x58c540 + count_ * 8, 4);
            // Actual425880 exists separately, but this owner has no canonical
            // animation attachment. Never skip it and silently parse L_TRACK.
            return {0x42c7b2, 0x425880, false, static_cast<std::uint32_t>(cursor)};
        }
        if (key_42c580(block, "L_TRACK", cursor))
        {
            token_42c5e0(block, cursor, observer, ordinal, 0x42c820);
            const auto name = append_;
            const auto index = count_;
            track_name_ = name;
            event(observer, ordinal, 0x42c838, 0x624460, 4);
            const auto extent = name_extent(0x42c83e); // source scans after alias publication
            records_[index].name = name;
            event(observer, ordinal, 0x42c842, 0x58c540 + index * 8, 4);
            records_[index].role = 3;
            event(observer, ordinal, 0x42c84a, 0x58c544 + index * 8, 4);
            count_ = index + 1;
            event(observer, ordinal, 0x42c85a, 0x58c808, 4);
            append_ = SourceRequestName(identity_, name.offset_ + extent);
            event(observer, ordinal, 0x42c860, 0x58c234, 4);
            records_[count_].name = {};
            event(observer, ordinal, 0x42c866, 0x58c540 + count_ * 8, 4);
        }
        return {0x42c872, 0, true, static_cast<std::uint32_t>(cursor)}; // only the source branch with no sky key
    }
    SourceRequestBoundary reset_42eb00_prefix(SourceRequestObserver *observer)
    {
        check_thread(0x42eb00);
        std::uint32_t ordinal = 0;
        for (std::uint32_t offset = 0; offset < names_.size(); offset += 4)
        {
            for (std::uint32_t byte = 0; byte < 4; ++byte)
                names_[offset + byte] = 0;
            event(observer, ordinal, 0x42eb0d, 0x58c238 + offset, 4);
        }
        for (std::uint32_t index = 0; index < records_.size(); ++index)
        {
            records_[index].name = {};
            event(observer, ordinal, 0x42eb19, 0x58c540 + index * 8, 4);
            records_[index].role = 0;
            event(observer, ordinal, 0x42eb19, 0x58c544 + index * 8, 4);
        }
        return {0x42eb1b, 0, false}; // next region is opaque58C810; not implemented
    }

  public:
    SourcePmdRequests()
        : identity_(std::make_shared<const detail::SourceRequestIdentity>(
              detail::SourceRequestIdentity{this, std::this_thread::get_id()}))
    {
    }
    SourcePmdRequests(const SourcePmdRequests &) = delete;
    SourcePmdRequests &operator=(const SourcePmdRequests &) = delete;
    SourcePmdRequests(SourcePmdRequests &&) = delete;
    SourcePmdRequests &operator=(SourcePmdRequests &&) = delete;
    SourceRequestValues snapshot() const
    {
        check_thread(0x42c740);
        return {names_, records_, append_, track_name_, count_};
    }
    // 42C7B7: one DWORD read from the existing58C808 owner. Thread check is NEW host admission.
    std::uint32_t read_count_58c808_42c7b7() const
    {
        check_thread(0x42c7b7);
        return count_;
    }
    // 42C7C2 reads only the name pointer value, indexed by the earlier EDX.
    // NEW admitted domain: a DWORD field in one of the existing24 records.
    // Null/expired names are pointer values here, not character-read failures.
    SourceRequestName read_record_name_42c7c2(std::uint32_t captured_count) const
    {
        check_thread(0x42c7c2);
        require(captured_count >= 1 && captured_count <= records_.size(), 0x42c7c2,
                "captured request index is outside the existing record region");
        return records_[captured_count - 1].name;
    }
    SourceRequestName track_name() const
    {
        check_thread(0x401f2e);
        return track_name_;
    }
};
inline std::shared_ptr<const detail::SourceRequestIdentity> SourceRequestName::checked(std::uint32_t site) const
{
    if (!nonnull_)
        throw SourceRequestError(site, "null source name pointer");
    const auto identity = owner_.lock();
    if (!identity)
        throw SourceRequestError(site, "expired source name region");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceRequestError(site, "source name region belongs to another thread");
    return identity;
}
inline SourceRequestName SourceRequestName::alias_value_462890(std::uint32_t address) const
{
    if (address == 0)
        return {};
    const auto offset = source_462890::difference(address, 0x58c238);
    // NEW result projection, not a retail memory read. Accept the sole region
    // and its one-past value; reject a foreign address without a host cast.
    if (!nonnull_ || offset > 768)
        throw SourceRequestError(0x462890, "result alias outside the same source name region");
    auto result = *this;
    result.offset_ = offset;
    return result;
}
inline std::uint8_t SourceRequestName::read_u8_462890(std::uint32_t address, std::uint32_t site) const
{
    if (!source_462890::haystack_byte_site(site))
        throw SourceRequestError(site, "not a reached462890 haystack byte site");
    const auto identity = checked(site);
    const auto offset = source_462890::difference(address, 0x58c238);
    if (offset >= identity->owner->names_.size())
        throw SourceRequestError(site, "reached source name byte outside region");
    return static_cast<std::uint8_t>(identity->owner->names_[offset]);
}
inline std::uint32_t SourceRequestName::read_u32le_462890(std::uint32_t address, std::uint32_t site) const
{
    if (!source_462890::haystack_dword_site(site))
        throw SourceRequestError(site, "not a reached464F96/464FD8 DWORD site");
    const auto identity = checked(site);
    const auto offset = source_462890::difference(address, 0x58c238);
    if (offset > identity->owner->names_.size() - 4)
        throw SourceRequestError(site, "reached source name DWORD outside region");
    // One logical four-byte read; no alignment-dependent host pointer cast.
    // Each call reads the live owner again, including the464FD8 reread.
    std::uint32_t value;
    std::memcpy(&value, identity->owner->names_.data() + offset, 4);
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
    if constexpr (std::endian::native == std::endian::big)
        value = std::byteswap(value);
    return value;
}
inline std::uint8_t SourceRequestName::read_formatter_byte(std::uint32_t relative, std::uint32_t site) const
{
    if (site != 0x464ba8 && site != 0x464efc)
        throw SourceRequestError(site, "not a reached camera formatter string read");
    const auto identity = checked(site);
    const auto at = std::uint64_t(offset_) + relative;
    if (at >= identity->owner->names_.size())
        throw SourceRequestError(site, "camera formatter name byte outside actual region");
    return static_cast<std::uint8_t>(identity->owner->names_[static_cast<std::size_t>(at)]);
}
inline char SourceRequestName::byte(std::uint32_t relative) const
{
    const auto identity = checked(0x42c5e0);
    const auto at = std::uint64_t(offset_) + relative;
    if (at >= identity->owner->names_.size())
        throw SourceRequestError(0x42c5e0, "source name read outside region");
    return identity->owner->names_[static_cast<std::size_t>(at)];
}
inline SourceRequestName SourceRequestName::advanced(std::uint32_t bytes) const
{
    const auto identity = checked(0x42c7a5);
    const auto next = std::uint64_t(offset_) + bytes;
    if (next > identity->owner->names_.size())
        throw SourceRequestError(0x42c7a5, "source name pointer exceeds one-past region");
    return SourceRequestName(identity, static_cast<std::uint32_t>(next));
}
inline std::string SourceRequestName::text_snapshot() const
{
    std::string result;
    for (std::uint32_t i = 0;; ++i)
    {
        const auto value = byte(i);
        if (value == 0)
            return result;
        result.push_back(value);
    }
}
} // namespace sgn::retail
