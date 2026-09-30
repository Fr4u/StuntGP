#pragma once
#include "sgn/retail_world_polygons.hpp"
#include <bit>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace sgn::retail
{
namespace detail
{
// Host lifetime metadata for an EXISTING packet array. No packet payload here.
struct WorldPolygonPacketStorageIdentity
{
    std::span<WorldPolygonPacket> storage;
    std::thread::id thread{std::this_thread::get_id()};
};
} // namespace detail
class WorldPolygonPacketStorageLifetime;
class WorldPolygonPacketPointer
{
    struct Binding
    {
        std::weak_ptr<const detail::WorldPolygonPacketStorageIdentity> identity;
        std::size_t offset{};
    };
    std::optional<Binding> binding_;
    struct SourceBinding
    {
        SourceWorldSpan<std::array<std::uint32_t, 5>> records;
        std::size_t offset{};
    };
    std::optional<SourceBinding> source_;
    void require_source_record() const
    {
        if (!source_ || source_->offset >= source_->records.size())
            throw FormatError("source packet record outside allocation");
    }
    friend class WorldPolygonPacketStorageLifetime;
    std::shared_ptr<const detail::WorldPolygonPacketStorageIdentity> require_identity() const
    {
        if (!binding_)
            throw FormatError("null world polygon packet pointer");
        auto identity = binding_->identity.lock();
        if (!identity)
            throw FormatError("expired world polygon packet storage");
        if (identity->thread != std::this_thread::get_id())
            throw FormatError("packet borrow used outside its owner thread");
        if (binding_->offset > identity->storage.size())
            throw FormatError("packet offset outside storage");
        return identity;
    }

  public:
    static WorldPolygonPacketPointer source(SourceWorldSpan<std::array<std::uint32_t, 5>> records)
    {
        if (!records.source_bound())
            throw FormatError("source packet pool requires its source allocation");
        WorldPolygonPacketPointer out;
        if (!records.source_binding().allocation)
            return out; // retail NULL publication, no dereference
        out.source_ = SourceBinding{std::move(records), 0};
        return out;
    }
    void require_source_platform(const std::shared_ptr<RaceSessionPlatform> &platform) const
    {
        if (!source_)
            throw FormatError("packet has no source platform");
        const auto &owner = source_->records.source_binding().platform;
        if (owner.owner_before(platform) || platform.owner_before(owner))
            throw FormatError("packet belongs to another platform");
        require_source_record();
    }
    bool source_bound() const noexcept
    {
        return source_.has_value();
    }
    bool is_null() const noexcept
    {
        return !binding_ && !source_;
    }
    std::optional<std::size_t> offset() const noexcept
    {
        if (source_)
            return source_->offset;
        return binding_ ? std::optional(binding_->offset) : std::nullopt;
    }
    bool same_storage(const WorldPolygonPacketPointer &other) const noexcept
    {
        if (source_ || other.source_)
            return source_ && other.source_ && source_->records.same_source_allocation(other.source_->records);
        return binding_ && other.binding_ && !binding_->identity.owner_before(other.binding_->identity) &&
               !other.binding_->identity.owner_before(binding_->identity);
    }
    bool same_pointer(const WorldPolygonPacketPointer &other) const noexcept
    {
        return (is_null() && other.is_null()) || (same_storage(other) && offset() == other.offset());
    }
    std::span<WorldPolygonPacket> storage() const
    {
        if (source_)
            throw FormatError("source20-byte packet pool has no host-structure span");
        return require_identity()->storage;
    }
    std::size_t storage_size() const
    {
        if (source_)
        {
            const auto count = source_->records.size();
            if (source_->offset > count)
                throw FormatError("source packet cursor outside allocation");
            return count;
        }
        return require_identity()->storage.size();
    }
    std::uint32_t source_word(unsigned lane) const
    {
        if (lane >= 5)
            throw FormatError("source packet word outside20-byte record");
        require_source_record();
        return source_->records[source_->offset][lane];
    }
    void store_source_word(unsigned lane, std::uint32_t word) const
    {
        if (lane >= 5)
            throw FormatError("source packet word outside20-byte record");
        require_source_record();
        source_->records[source_->offset][lane] = word;
    }
    void require_storage(const WorldPolygonPacketStorageLifetime &) const;
    WorldPolygonPacket &record() const
    {
        if (source_)
            throw FormatError("source packet needs checked pointer-field adapters before native use");
        const auto identity = require_identity();
        if (binding_->offset == identity->storage.size())
            throw FormatError("source world polygon packet capacity");
        return identity->storage[binding_->offset];
    }
    // Local register advance only. The caller publishes its source current.
    void advance()
    {
        if (source_)
        {
            require_source_record();
            ++source_->offset;
            return;
        }
        (void)record();
        ++binding_->offset;
    }
    std::span<WorldPolygonPacket> range(std::uint32_t count) const
    {
        if (is_null())
        {
            if (count)
                throw FormatError("null packet range has nonzero count");
            return {};
        }
        if (source_)
            throw FormatError("source packet range requires a source record consumer, not host struct aliasing");
        const auto identity = require_identity();
        if (count > identity->storage.size() - binding_->offset)
            throw FormatError("packet range outside allocation");
        return identity->storage.subspan(binding_->offset, count);
    }
    // NEW host performance (perf_world R19, COORDINATION.md entry 1). A
    // synchronous run over ONE pointer object that memoizes its checked source
    // record view. The first reached view performs the existing SourceWorldSpan
    // admission (platform lock, bank thread/generation/extent/opaque checks) at
    // exactly the point the uncached member would; later calls reuse that span
    // and keep every per-call check (lane, record/offset, platform identity)
    // with the same messages. Contract: while the run is alive the pointer is
    // changed only through it, no bank allocation is allocated or released and
    // no opaque transport word is stored into the packet pool.
    class SourceRun
    {
        WorldPolygonPacketPointer &pointer_;
        std::span<std::array<std::uint32_t, 5>> records_{};
        bool viewed_{};
        std::size_t size()
        {
            if (!viewed_)
            {
                records_ = pointer_.source_->records.view();
                viewed_ = true;
            }
            return records_.size();
        }
        void require_record()
        { //== require_source_record()
            if (!pointer_.source_ || pointer_.source_->offset >= size())
                throw FormatError("source packet record outside allocation");
        }

      public:
        explicit SourceRun(WorldPolygonPacketPointer &pointer) noexcept : pointer_(pointer) {}
        SourceRun(const SourceRun &) = delete;
        SourceRun &operator=(const SourceRun &) = delete;
        const WorldPolygonPacketPointer &pointer() const noexcept
        {
            return pointer_;
        }
        void require_source_platform(const std::shared_ptr<RaceSessionPlatform> &platform)
        {
            if (!pointer_.source_)
                throw FormatError("packet has no source platform");
            const auto &owner = pointer_.source_->records.source_binding().platform;
            if (owner.owner_before(platform) || platform.owner_before(owner))
                throw FormatError("packet belongs to another platform");
            require_record();
        }
        std::uint32_t source_word(unsigned lane)
        {
            if (lane >= 5)
                throw FormatError("source packet word outside20-byte record");
            require_record();
            return records_[pointer_.source_->offset][lane];
        }
        void store_source_word(unsigned lane, std::uint32_t word)
        {
            if (lane >= 5)
                throw FormatError("source packet word outside20-byte record");
            require_record();
            records_[pointer_.source_->offset][lane] = word;
        }
        void advance()
        {
            if (pointer_.source_)
            {
                require_record();
                ++pointer_.source_->offset;
                return;
            }
            pointer_.advance();
        }
    };
};
// Full427520 through the active representation; only the first DWORD changes.
inline void reject_world_polygon_packet_427520(const WorldPolygonPacketPointer &pointer)
{
    if (pointer.source_bound())
        pointer.store_source_word(0, 0);
    else
        reject_world_polygon(pointer.record());
}
class WorldPolygonPacketStorageLifetime
{
    std::shared_ptr<const detail::WorldPolygonPacketStorageIdentity> identity_;
    static std::span<WorldPolygonPacket> checked_storage(std::span<WorldPolygonPacket> storage)
    {
        (void)checked_extent(storage.size());
        return storage;
    }

  public:
    // Native representable signed byte-difference domain; not a source cap.
    static constexpr std::size_t maximum_elements = std::numeric_limits<std::int32_t>::max() / 20;
    static std::size_t checked_extent(std::size_t extent)
    {
        if (extent > maximum_elements)
            throw FormatError("unsupported native packet difference extent");
        return extent;
    }
    explicit WorldPolygonPacketStorageLifetime(std::span<WorldPolygonPacket> storage)
        : identity_(std::make_shared<detail::WorldPolygonPacketStorageIdentity>(checked_storage(storage)))
    {
    }
    ~WorldPolygonPacketStorageLifetime()
    {
        if (identity_->thread != std::this_thread::get_id())
            std::terminate();
    }
    WorldPolygonPacketStorageLifetime(const WorldPolygonPacketStorageLifetime &) = delete;
    WorldPolygonPacketStorageLifetime &operator=(const WorldPolygonPacketStorageLifetime &) = delete;
    WorldPolygonPacketStorageLifetime(WorldPolygonPacketStorageLifetime &&) = delete;
    WorldPolygonPacketStorageLifetime &operator=(WorldPolygonPacketStorageLifetime &&) = delete;
    WorldPolygonPacketPointer pointer(std::size_t offset = 0) const
    {
        if (identity_->thread != std::this_thread::get_id())
            throw FormatError("packet publication outside owner thread");
        if (offset > identity_->storage.size())
            throw FormatError("packet publication outside storage");
        WorldPolygonPacketPointer result;
        result.binding_ = WorldPolygonPacketPointer::Binding{identity_, offset};
        return result;
    }
};
inline void WorldPolygonPacketPointer::require_storage(const WorldPolygonPacketStorageLifetime &owner) const
{
    (void)require_identity();
    if (!same_storage(owner.pointer()))
        throw FormatError("packet pointer belongs to a foreign pool");
}
// 42B180/200: source SUB followed by signed division by20. The supported native
// domain is aligned elements of the same bounded owner; negative stays negative.
inline std::int32_t world_polygon_packet_difference(const WorldPolygonPacketPointer &first,
                                                    const WorldPolygonPacketPointer &current)
{
    if (first.is_null() && current.is_null())
        return 0;
    (void)first.storage_size();
    (void)current.storage_size();
    if (!first.same_storage(current))
        throw FormatError("mixed-owner packet difference");
    const auto difference = static_cast<std::int64_t>(*current.offset()) - static_cast<std::int64_t>(*first.offset());
    return static_cast<std::int32_t>((difference * 20) / 20);
}
class WorldPolygonProducerList
{
    WorldPolygonPacketPointer base_, current_;
    std::thread::id thread_{std::this_thread::get_id()};

  public:
    WorldPolygonProducerList() = default;
    // Explicit fixture/owner bindings. No span/count compatibility constructor.
    WorldPolygonProducerList(WorldPolygonPacketPointer base, WorldPolygonPacketPointer current)
        : base_(std::move(base)), current_(std::move(current))
    {
    }
    WorldPolygonProducerList(const WorldPolygonProducerList &) = delete;
    WorldPolygonProducerList &operator=(const WorldPolygonProducerList &) = delete;
    WorldPolygonProducerList(WorldPolygonProducerList &&) = delete;
    WorldPolygonProducerList &operator=(WorldPolygonProducerList &&) = delete;
    void require_owner_thread() const
    {
        if (thread_ != std::this_thread::get_id())
            throw FormatError("packet cursor used outside its owner thread");
    }
    WorldPolygonPacketPointer base() const noexcept
    {
        return base_;
    }
    WorldPolygonPacketPointer current() const noexcept
    {
        return current_;
    }
    void publish_base(WorldPolygonPacketPointer p)
    {
        require_owner_thread();
        base_ = std::move(p);
    }
    void publish_current(WorldPolygonPacketPointer p)
    {
        require_owner_thread();
        current_ = std::move(p);
    }
    void clear_base()
    {
        require_owner_thread();
        base_ = {};
    }
    void copy_base_to_current()
    {
        require_owner_thread();
        current_ = base_;
    }
    // Derived reporting only. Never a second source word or writable counter.
    std::uint32_t count() const
    {
        require_owner_thread();
        return std::bit_cast<std::uint32_t>(world_polygon_packet_difference(base_, current_));
    }
    std::span<WorldPolygonPacket> prefix() const
    {
        return base_.range(count());
    }
};
} // namespace sgn::retail
