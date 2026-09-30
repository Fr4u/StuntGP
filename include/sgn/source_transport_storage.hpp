#pragma once
#include "sgn/source_transport_word.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
namespace sgn::retail
{
// One physical payload, byte-level validity; no shadow numerical world.
// Known initial state is selected only for source PE zero-initialized owners.
// NEW R19 (perf_transport): shipped game (SGN_SOURCE_PROVENANCE=0) = plain T
// storage with checked byte extents; unknown() writes the documented 0 default
// of unproduced source DWORDs. Diagnostic build (=1): a byte is opaque iff its
// metadata origin is present; opaque_count_ lets whole-owner borrows skip the
// scan when no byte is opaque.
template <class T> class SourceTransportStorage
{
    static_assert(std::is_trivially_copyable_v<T>);
    T payload_{};
#if SGN_SOURCE_PROVENANCE
    std::array<SourceTransportByte, sizeof(T)> opaque_{};
    std::uint32_t opaque_count_{};
    void set_meta(std::uint32_t at, const SourceTransportByte &value) noexcept
    {
        if (!opaque_[at].origin)
            ++opaque_count_;
        opaque_[at] = value;
    }
    void clear_meta(std::uint32_t at) noexcept
    {
        if (opaque_[at].origin)
        {
            opaque_[at] = {};
            --opaque_count_;
        }
    }
#endif
    void extent(std::uint32_t at, std::uint32_t n, std::uint32_t site) const
    {
        if (at > sizeof(T) || n > sizeof(T) - at)
            throw SourceAllocationError(site, "source local/static transport leaves actual owner");
    }

  public:
    explicit SourceTransportStorage() = default;
    void unknown(std::uint32_t function, std::int32_t base)
    {
        // Same word order, extents and (diagnostic) origins as store(i,opaque(function,0,base+i)).
        for (std::uint32_t i = 0; i < sizeof(T); i += 4)
            store(i, SourceTransportWord::opaque(function, 0, base + static_cast<std::int32_t>(i)), function);
    }
    SourceTransportWord load(std::uint32_t at, std::uint32_t site) const
    {
        extent(at, 4, site);
        SourceTransportWord v{};
        const auto *b = reinterpret_cast<const std::uint8_t *>(&payload_);
#if SGN_SOURCE_PROVENANCE
        if (opaque_count_)
        {
            for (unsigned j = 0; j < 4; ++j)
                v.bytes[j] = opaque_[at + j].origin ? opaque_[at + j] : SourceTransportByte{b[at + j], 0, {}};
            return v;
        }
#endif
        for (unsigned j = 0; j < 4; ++j)
            v.bytes[j].literal = b[at + j];
        return v;
    }
    void store(std::uint32_t at, const SourceTransportWord &value, std::uint32_t site)
    {
        extent(at, 4, site);
        auto *b = reinterpret_cast<std::uint8_t *>(&payload_);
#if SGN_SOURCE_PROVENANCE
        for (unsigned j = 0; j < 4; ++j)
        {
            if (value.bytes[j].origin)
                set_meta(at + j, value.bytes[j]);
            else
            {
                b[at + j] = value.bytes[j].literal;
                clear_meta(at + j);
            }
        }
#else
        for (unsigned j = 0; j < 4; ++j)
            b[at + j] = value.bytes[j].literal;
#endif
    }
    void store(std::uint32_t at, std::uint32_t value, std::uint32_t site)
    {
        store(at, SourceTransportWord::known(value), site);
    }
    void store_byte(std::uint32_t at, std::uint8_t value, std::uint32_t site)
    {
        extent(at, 1, site);
        reinterpret_cast<std::uint8_t *>(&payload_)[at] = value;
#if SGN_SOURCE_PROVENANCE
        clear_meta(at);
#endif
    }
    std::uint8_t byte(std::uint32_t at, std::uint32_t site) const
    {
        extent(at, 1, site);
#if SGN_SOURCE_PROVENANCE
        if (opaque_[at].origin)
            throw SourceOpaqueRead(site, opaque_[at].origin);
#endif
        return reinterpret_cast<const std::uint8_t *>(&payload_)[at];
    }
    std::uint32_t number(std::uint32_t at, std::uint32_t site) const
    {
        return load(at, site).numeric(site);
    }
    const T &all(std::uint32_t site = 0) const
    {
#if SGN_SOURCE_PROVENANCE
        if (opaque_count_)
            for (const auto &b : opaque_)
                if (b.origin)
                    throw SourceOpaqueRead(site, b.origin);
#else
        (void)site;
#endif
        return payload_;
    }
    operator const T &() const
    {
        return all();
    }
    operator T &()
    {
        (void)all();
        return payload_;
    }
    const void *qualified(std::span<const unsigned> offsets, std::uint32_t site) const
    {
        for (auto off : offsets)
            (void)number(off, site);
        return &payload_;
    }
};
} // namespace sgn::retail
