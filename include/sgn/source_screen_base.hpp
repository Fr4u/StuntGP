#pragma once
#include "sgn/retail_screen_polygons.hpp"
#include <cstdint>
#include <memory>
#include <span>
#include "sgn/source_world_span.hpp"

namespace sgn::retail
{
class RaceSessionPlatform;
struct RaceSessionState;
struct SourceScreenBaseStoreObserver;
struct SourceSceneScreenBaseResult
{
    std::uint32_t next_required_instruction{0x42e578};
};
struct SourceScreenBasePublicationResult
{
    std::uint32_t next_required_instruction{0x42e4e1};
};

// Host representation of the sole screen payload. Switching to source storage
// destroys the native array alternative. The bank remains the sole allocator.
class ScreenPolygonStorage
{
    std::variant<std::array<SourceScreenPolygon, 256>, SourceWorldSpan<SourceScreenPolygon>> storage_;
    friend SourceScreenBasePublicationResult allocate_source_screen_base_42e4d0(
        const std::weak_ptr<RaceSessionPlatform> &, SourceScreenBaseStoreObserver *);
    void bind_source(SourceWorldSpan<SourceScreenPolygon> source)
    {
        storage_.emplace<1>(std::move(source));
    }

  public:
    using value_type = SourceScreenPolygon;
    const SourceWorldBufferBinding &source_binding() const
    {
        return std::get<1>(storage_).source_binding();
    }
    bool source_bound() const noexcept
    {
        return storage_.index() == 1;
    }
    std::span<SourceScreenPolygon, 256> view()
    {
        if (auto *native = std::get_if<0>(&storage_))
            return std::span<SourceScreenPolygon, 256>{*native};
        const auto source = std::get<1>(storage_).view();
        if (source.size() != 256)
            throw SourceAllocationError(0x42e4dc, "screen source extent mismatch");
        return std::span<SourceScreenPolygon, 256>{source.data(), 256};
    }
    std::span<const SourceScreenPolygon, 256> view() const
    {
        if (const auto *native = std::get_if<0>(&storage_))
            return std::span<const SourceScreenPolygon, 256>{*native};
        const auto source = std::get<1>(storage_).view();
        if (source.size() != 256)
            throw SourceAllocationError(0x42e4dc, "screen source extent mismatch");
        return std::span<const SourceScreenPolygon, 256>{source.data(), 256};
    }
    auto data()
    {
        return view().data();
    }
    auto data() const
    {
        return view().data();
    }
    auto begin()
    {
        return view().begin();
    }
    auto begin() const
    {
        return view().begin();
    }
    auto end()
    {
        return view().end();
    }
    auto end() const
    {
        return view().end();
    }
    auto size() const
    {
        return view().size();
    }
    SourceScreenPolygon &operator[](std::size_t i)
    {
        if (i >= 256)
            throw SourceAllocationError(0x42e4dc, "screen index outside owner");
        return view()[i];
    }
    const SourceScreenPolygon &operator[](std::size_t i) const
    {
        if (i >= 256)
            throw SourceAllocationError(0x42e4dc, "screen index outside owner");
        return view()[i];
    }
};

// Nullable publication to this world's sole native or source screen payload.
class ScreenPolygonBasePublication
{
    friend SourceScreenBasePublicationResult allocate_source_screen_base_42e4d0(
        const std::weak_ptr<RaceSessionPlatform> &, SourceScreenBaseStoreObserver *);
    bool owner_begin_{}; // the pinned PE loader zero-fills61C5F0
    friend SourceSceneScreenBaseResult reset_source_scene_screen_base_42e572(const std::weak_ptr<RaceSessionPlatform> &,
                                                                             SourceScreenBaseStoreObserver *);
    friend SourceScreenBasePublicationResult publish_source_screen_base_42e4dc(
        const std::weak_ptr<RaceSessionPlatform> &, SourceScreenBaseStoreObserver *);

  public:
    // Read only through an already valid world reference or locked access.
    bool is_null() const noexcept
    {
        return !owner_begin_;
    }
};

// Synchronous read-only observation AFTER the single source publication.
// An exception keeps that store committed. No callback replay or rollback.
struct SourceScreenBaseStoreObserver
{
    virtual ~SourceScreenBaseStoreObserver() = default;
    virtual void stored(std::uint32_t instruction, std::uint32_t address, const RaceSessionState &) = 0;
};

// Scope-bound access retains the EXISTING platform control block. It does not
// create a shared owner, retain a new copy of the world, or snapshot the base.
// Any returned span/reference is valid only within this access object's scope.
class ScreenPolygonOwnerAccess
{
    std::shared_ptr<RaceSessionPlatform> platform_;
    explicit ScreenPolygonOwnerAccess(std::shared_ptr<RaceSessionPlatform>);
    RaceSessionState &checked_world() const;
    friend ScreenPolygonOwnerAccess lock_screen_polygon_owner(const std::weak_ptr<RaceSessionPlatform> &);

  public:
    ~ScreenPolygonOwnerAccess() = default;
    ScreenPolygonOwnerAccess(const ScreenPolygonOwnerAccess &) = delete;
    ScreenPolygonOwnerAccess &operator=(const ScreenPolygonOwnerAccess &) = delete;
    ScreenPolygonOwnerAccess(ScreenPolygonOwnerAccess &&) = delete;
    ScreenPolygonOwnerAccess &operator=(ScreenPolygonOwnerAccess &&) = delete;
    bool base_is_null() const;
    std::optional<SourceWorldBufferBinding> published_source() const;
    std::span<SourceScreenPolygon, 256> retained_records() const;
    // Read the publication at the SOURCE read point, not at outer-frame entry.
    // NULL is valid retained state, but never an empty drawable span.
    std::span<SourceScreenPolygon, 256> published_records() const;
    std::uint32_t &count_word() const;
};

ScreenPolygonOwnerAccess lock_screen_polygon_owner(const std::weak_ptr<RaceSessionPlatform> &);
// These readers borrow access; keep it alive through the synchronous consumer.
// Construction does not read count/publication. Each read validates the owner.
MutableScreenPolygonBaseReader live_screen_polygon_reader(const ScreenPolygonOwnerAccess &);
ConstScreenPolygonBaseReader live_screen_polygon_const_reader(const ScreenPolygonOwnerAccess &);
// Exactly42E572. Preserves count, all records and every other world owner.
SourceSceneScreenBaseResult reset_source_scene_screen_base_42e572(const std::weak_ptr<RaceSessionPlatform> &,
                                                                  SourceScreenBaseStoreObserver * = nullptr);
// Native host-array publication only; rejects source-bound storage.
// Exactly the42E4DC publication to this EXISTING typed array. It does not call
// 42E8D0, initialize payload, change count, reconstruct13 allocations or admit430.
SourceScreenBasePublicationResult publish_source_screen_base_42e4dc(const std::weak_ptr<RaceSessionPlatform> &,
                                                                    SourceScreenBaseStoreObserver * = nullptr);
// Actual tenth allocation42E4D0 and publication42E4DC. Count is retained.
SourceScreenBasePublicationResult allocate_source_screen_base_42e4d0(const std::weak_ptr<RaceSessionPlatform> &,
                                                                     SourceScreenBaseStoreObserver * = nullptr);
} // namespace sgn::retail
