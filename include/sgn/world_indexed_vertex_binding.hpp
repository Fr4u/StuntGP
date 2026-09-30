#pragma once
#include "sgn/retail_world_model_producer.hpp"
#include <limits>
#include <exception>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace sgn::retail
{
namespace detail
{
// Host borrow metadata only. It owns no vertex or pointer-array payload.
struct WorldIndexedVertexStorageIdentity
{
    std::span<WorldPolygonVertex *> storage;
    std::thread::id thread{std::this_thread::get_id()};
};
} // namespace detail
class WorldIndexedVertexStorageLifetime;
class WorldIndexedVertexPointer
{
    struct Binding
    {
        std::weak_ptr<const detail::WorldIndexedVertexStorageIdentity> identity;
        std::size_t offset;
    };
    std::optional<Binding> binding_;
    struct SourceBinding
    {
        SourceWorldSpan<std::uint32_t> words;
        SourceWorldSpan<WorldPolygonVertex> vertices;
        std::size_t offset{};
    };
    std::optional<SourceBinding> source_;
    void require_source() const
    {
        if (!source_ || source_->offset > source_->words.size())
            throw FormatError("source indexed cursor outside allocation");
    }
    friend class WorldIndexedVertexStorageLifetime;
    std::shared_ptr<const detail::WorldIndexedVertexStorageIdentity> require_identity() const
    {
        if (!binding_)
            throw FormatError("null indexed vertex pointer");
        auto identity = binding_->identity.lock();
        if (!identity)
            throw FormatError("expired indexed vertex storage");
        if (identity->thread != std::this_thread::get_id())
            throw FormatError("indexed vertex borrow used outside its owner thread");
        if (binding_->offset > identity->storage.size())
            throw FormatError("indexed vertex offset outside storage");
        return identity;
    }

  public:
    static WorldIndexedVertexPointer source(SourceWorldSpan<std::uint32_t> words,
                                            SourceWorldSpan<WorldPolygonVertex> vertices)
    {
        if (!words.source_bound() || !vertices.source_bound())
            throw FormatError("source indexed bindings require source allocations");
        WorldIndexedVertexPointer out;
        if (!words.source_binding().allocation)
            return out; // actual published NULL, no payload dereference
        out.source_ = SourceBinding{std::move(words), std::move(vertices), 0};
        return out;
    }
    void require_source_vertices(const SourceWorldSpan<WorldPolygonVertex> &vertices) const
    {
        if (!source_ || !source_->vertices.same_source_allocation(vertices))
            throw FormatError("source indexed pointer belongs to another vertex allocation");
        const auto &words = source_->words.source_binding();
        const auto &points = vertices.source_binding();
        if (words.bank != points.bank || words.platform.owner_before(points.platform) ||
            points.platform.owner_before(words.platform))
            throw FormatError("source indexed word and vertex allocations belong to different platforms");
        require_source();
        (void)source_->vertices.size(); // actual generations, extents and thread
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
    bool same_storage(const WorldIndexedVertexPointer &other) const noexcept
    {
        if (source_ || other.source_)
            return source_ && other.source_ && source_->words.same_source_allocation(other.source_->words) &&
                   source_->vertices.same_source_allocation(other.source_->vertices);
        return binding_ && other.binding_ && !binding_->identity.owner_before(other.binding_->identity) &&
               !other.binding_->identity.owner_before(binding_->identity);
    }
    bool same_pointer(const WorldIndexedVertexPointer &other) const noexcept
    {
        return (is_null() && other.is_null()) || (same_storage(other) && offset() == other.offset());
    }
    std::span<WorldPolygonVertex *> storage() const
    {
        if (source_)
            throw FormatError("source DWORD indexed storage cannot be borrowed as host-pointer span");
        return require_identity()->storage;
    }
    std::size_t storage_size() const
    {
        if (source_)
        {
            require_source();
            return source_->words.size();
        }
        return require_identity()->storage.size();
    }
    WorldPolygonVertex *vertex_at(std::size_t absolute_index) const
    {
        if (!source_)
        {
            const auto values = storage();
            if (absolute_index >= values.size())
                throw FormatError("indexed read outside owner");
            return values[absolute_index];
        }
        require_source();
        const auto word = source_->words[absolute_index];
        if (word == 0)
            return nullptr;
        // Native address transport: nonzero32-aligned vertex-relative identity.
        // The source allocation identity is retained in this binding, never in a second payload array.
        if (word < 32 || (word - 32) % 32 != 0)
            throw FormatError("invalid normalized indexed vertex identity");
        const auto index = (word - 32) / 32;
        const auto vertices = source_->vertices.view();
        if (index >= vertices.size())
            throw FormatError("indexed vertex identity outside its retained source allocation");
        return &vertices[index];
    }
    void require_storage(const WorldIndexedVertexStorageLifetime &) const;
    // Invocation-local EDI/ESI advance. Source global publication is separate.
    void append(WorldPolygonVertex *vertex)
    {
        if (source_)
        {
            require_source();
            if (source_->offset >= source_->words.size())
                throw FormatError("source indexed vertex capacity");
            std::uint32_t word = 0;
            if (vertex)
            {
                const auto vertices = source_->vertices.view();
                const auto start = reinterpret_cast<std::uintptr_t>(vertices.data());
                const auto address = reinterpret_cast<std::uintptr_t>(vertex);
                if (address < start || address - start >= vertices.size_bytes() || (address - start) % 32 != 0 ||
                    address - start > std::numeric_limits<std::uint32_t>::max() - 32u)
                    throw FormatError("indexed append points outside the retained source vertex allocation");
                word = static_cast<std::uint32_t>(address - start) + 32u;
            }
            source_->words[source_->offset] = word; // one actual4-byte store
            ++source_->offset;
            return;
        }
        const auto identity = require_identity();
        if (binding_->offset == identity->storage.size())
            throw FormatError("source indexed vertex capacity");
        identity->storage[binding_->offset] = vertex;
        ++binding_->offset;
    }
    // NEW host performance (perf_world R19, COORDINATION.md entry 1). A
    // synchronous append run over ONE pointer object. The word and vertex
    // views are admitted by the existing SourceWorldSpan checks at the first
    // point append() reaches them; later appends reuse those spans and keep
    // the cursor/capacity/vertex-range checks with the same messages. Native
    // pointers forward to append(). Contract: while the run is alive the
    // pointer changes only through it, no bank allocation is allocated or
    // released, and no opaque transport word is stored into either allocation.
    class SourceAppendRun
    {
        WorldIndexedVertexPointer &pointer_;
        std::span<std::uint32_t> words_{};
        std::span<WorldPolygonVertex> vertices_{};
        bool words_viewed_{}, vertices_viewed_{};
        std::size_t word_count()
        {
            if (!words_viewed_)
            {
                words_ = pointer_.source_->words.view();
                words_viewed_ = true;
            }
            return words_.size();
        }
        std::span<WorldPolygonVertex> vertices()
        {
            if (!vertices_viewed_)
            {
                vertices_ = pointer_.source_->vertices.view();
                vertices_viewed_ = true;
            }
            return vertices_;
        }

      public:
        explicit SourceAppendRun(WorldIndexedVertexPointer &pointer) noexcept : pointer_(pointer) {}
        SourceAppendRun(const SourceAppendRun &) = delete;
        SourceAppendRun &operator=(const SourceAppendRun &) = delete;
        void append(WorldPolygonVertex *vertex)
        {
            if (!pointer_.source_)
            {
                pointer_.append(vertex);
                return;
            }
            auto &source = *pointer_.source_;
            if (source.offset > word_count())
                throw FormatError("source indexed cursor outside allocation"); // require_source()
            if (source.offset >= word_count())
                throw FormatError("source indexed vertex capacity");
            std::uint32_t word = 0;
            if (vertex)
            {
                const auto points = vertices();
                const auto start = reinterpret_cast<std::uintptr_t>(points.data());
                const auto address = reinterpret_cast<std::uintptr_t>(vertex);
                if (address < start || address - start >= points.size_bytes() || (address - start) % 32 != 0 ||
                    address - start > std::numeric_limits<std::uint32_t>::max() - 32u)
                    throw FormatError("indexed append points outside the retained source vertex allocation");
                word = static_cast<std::uint32_t>(address - start) + 32u;
            }
            words_[source.offset] = word; // one actual4-byte store
            ++source.offset;
        }
    };
    // NEW host performance (perf_world R19): synchronous read run over ONE
    // pointer object; vertex_at() returns exactly what the member returns and
    // throws the same errors, admitting each SourceWorldSpan view at the first
    // point the member reached it. Same contract as SourceAppendRun.
    class SourceReadRun
    {
        const WorldIndexedVertexPointer &pointer_;
        std::span<std::uint32_t> words_{};
        std::span<WorldPolygonVertex> vertices_{};
        bool words_viewed_{}, vertices_viewed_{};

      public:
        explicit SourceReadRun(const WorldIndexedVertexPointer &pointer) noexcept : pointer_(pointer) {}
        SourceReadRun(const SourceReadRun &) = delete;
        SourceReadRun &operator=(const SourceReadRun &) = delete;
        WorldPolygonVertex *vertex_at(std::size_t absolute_index)
        {
            if (!pointer_.source_)
                return pointer_.vertex_at(absolute_index);
            const auto &source = *pointer_.source_;
            if (!words_viewed_)
            {
                words_ = source.words.view();
                words_viewed_ = true;
            }
            if (source.offset > words_.size())
                throw FormatError("source indexed cursor outside allocation"); // require_source()
            if (absolute_index >= words_.size())                               //== SourceWorldSpan::operator[]
                throw SourceAllocationError(0x42e430, "source world buffer index outside owner");
            const auto word = words_[absolute_index];
            if (word == 0)
                return nullptr;
            if (word < 32 || (word - 32) % 32 != 0)
                throw FormatError("invalid normalized indexed vertex identity");
            const auto index = (word - 32) / 32;
            if (!vertices_viewed_)
            {
                vertices_ = source.vertices.view();
                vertices_viewed_ = true;
            }
            if (index >= vertices_.size())
                throw FormatError("indexed vertex identity outside its retained source allocation");
            return &vertices_[index];
        }
    };
};
// Embed in the existing payload owner after its array, or scope a standalone
// fixture's borrow to an array that outlives it. Destruction expires all borrows.
class WorldIndexedVertexStorageLifetime
{
    std::shared_ptr<const detail::WorldIndexedVertexStorageIdentity> identity_;
    static std::span<WorldPolygonVertex *> checked_storage(std::span<WorldPolygonVertex *> storage)
    {
        (void)checked_native_projection_extent(storage.size());
        return storage;
    }

  public:
    // Supported native relative-projection domain, not a source allocator cap.
    static constexpr std::size_t maximum_native_projection_elements = std::numeric_limits<std::int32_t>::max() / 4;
    static std::size_t checked_native_projection_extent(std::size_t extent)
    {
        if (extent > maximum_native_projection_elements)
            throw FormatError("unsupported native indexed projection extent");
        return extent;
    }
    explicit WorldIndexedVertexStorageLifetime(std::span<WorldPolygonVertex *> storage)
        : identity_(std::make_shared<detail::WorldIndexedVertexStorageIdentity>(checked_storage(storage)))
    {
    }
    ~WorldIndexedVertexStorageLifetime()
    {
        if (identity_->thread != std::this_thread::get_id())
            std::terminate();
    }
    WorldIndexedVertexStorageLifetime(const WorldIndexedVertexStorageLifetime &) = delete;
    WorldIndexedVertexStorageLifetime &operator=(const WorldIndexedVertexStorageLifetime &) = delete;
    WorldIndexedVertexStorageLifetime(WorldIndexedVertexStorageLifetime &&) = delete;
    WorldIndexedVertexStorageLifetime &operator=(WorldIndexedVertexStorageLifetime &&) = delete;
    WorldIndexedVertexPointer pointer(std::size_t offset = 0) const
    {
        if (identity_->thread != std::this_thread::get_id())
            throw FormatError("indexed storage publication outside its owner thread");
        if (offset > identity_->storage.size())
            throw FormatError("indexed pointer publication outside storage");
        WorldIndexedVertexPointer result;
        result.binding_ = WorldIndexedVertexPointer::Binding{identity_, offset};
        return result;
    }
};
inline void WorldIndexedVertexPointer::require_storage(const WorldIndexedVertexStorageLifetime &owner) const
{
    (void)require_identity();
    if (!same_storage(owner.pointer()))
        throw FormatError("indexed pointer belongs to a foreign storage owner");
}
class WorldIndexedVertexCursor
{
    WorldIndexedVertexPointer base_, current_;
    std::thread::id thread_{std::this_thread::get_id()};
    void require_thread() const
    {
        if (thread_ != std::this_thread::get_id())
            throw FormatError("indexed cursor used outside its owner thread");
    }

  public:
    void require_owner_thread() const
    {
        require_thread();
    }
    WorldIndexedVertexPointer base() const noexcept
    {
        return base_;
    }
    WorldIndexedVertexPointer current() const noexcept
    {
        return current_;
    }
    void publish_base(WorldIndexedVertexPointer pointer)
    {
        require_thread();
        base_ = std::move(pointer);
    }
    void publish_current(WorldIndexedVertexPointer pointer)
    {
        require_thread();
        current_ = std::move(pointer);
    }
    void clear_base()
    {
        require_thread();
        base_ = {};
    } // 42E53C, only61C5EC
    void clear_current()
    {
        require_thread();
        current_ = {};
    } // representation; no new42E5D0 runtime caller
    void copy_base_to_current()
    {
        require_thread();
        current_ = base_;
    } // 42ACE7 /42E810, including NULL copy
    std::span<WorldPolygonVertex *> projection_vertices() const
    {
        require_thread();
        if (base_.is_null() && current_.is_null())
            return {}; // SUB0, SAR2, JLE
        if (base_.is_null() || current_.is_null())
            throw FormatError("unsupported null/live indexed projection domain");
        const auto storage = base_.storage();
        (void)current_.storage();
        if (!base_.same_storage(current_))
            throw FormatError("unsupported mixed-owner indexed projection domain");
        const auto first = *base_.offset(), last = *current_.offset();
        if (last <= first)
            return {}; // signed nonpositive source difference; no pointer subtraction
        return storage.subspan(first, last - first);
    }
    std::uint32_t count() const
    {
        require_thread();
        if (base_.is_null() && current_.is_null())
            return 0;
        if (base_.is_null() || current_.is_null())
            throw FormatError("unsupported null/live indexed projection domain");
        (void)base_.storage_size();
        (void)current_.storage_size();
        if (!base_.same_storage(current_))
            throw FormatError("unsupported mixed-owner indexed projection domain");
        const auto first = *base_.offset(), last = *current_.offset();
        if (last <= first)
            return 0;
        if (last - first > std::numeric_limits<std::uint32_t>::max())
            throw FormatError("indexed count exceeds source DWORD");
        return static_cast<std::uint32_t>(last - first);
    }
    WorldPolygonVertex *projection_vertex(std::uint32_t index) const
    {
        if (index >= count())
            throw FormatError("indexed projection element outside current range");
        return base_.vertex_at(*base_.offset() + index);
    }
};
} // namespace sgn::retail
