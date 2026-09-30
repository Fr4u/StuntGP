#include "sgn/source_screen_base.hpp"
#include "sgn/race_session.hpp"
#include <utility>

namespace sgn::retail
{
namespace
{
std::shared_ptr<RaceSessionPlatform> require_world(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    auto platform = borrow.lock();
    if (!platform)
        throw FormatError("screen base platform expired");
    platform->require_owner_thread();
    return platform;
}
} // namespace
ScreenPolygonOwnerAccess::ScreenPolygonOwnerAccess(std::shared_ptr<RaceSessionPlatform> platform)
    : platform_(std::move(platform))
{
}
RaceSessionState &ScreenPolygonOwnerAccess::checked_world() const
{
    platform_->require_owner_thread();
    return platform_->state();
}
bool ScreenPolygonOwnerAccess::base_is_null() const
{
    return checked_world().screen_base.is_null();
}
std::span<SourceScreenPolygon, 256> ScreenPolygonOwnerAccess::retained_records() const
{
    return checked_world().screen_polygons.view();
}
std::span<SourceScreenPolygon, 256> ScreenPolygonOwnerAccess::published_records() const
{
    auto &state = checked_world();
    if (state.screen_base.is_null())
        throw FormatError("screen base is NULL");
    return state.screen_polygons.view();
}
std::optional<SourceWorldBufferBinding> ScreenPolygonOwnerAccess::published_source() const
{
    auto &state = checked_world();
    if (state.screen_base.is_null())
        throw FormatError("screen base is NULL");
    if (!state.screen_polygons.source_bound())
        return std::nullopt;
    return state.screen_polygons.source_binding();
}
std::uint32_t &ScreenPolygonOwnerAccess::count_word() const
{
    return checked_world().screen_polygon_count;
}
ScreenPolygonOwnerAccess lock_screen_polygon_owner(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    return ScreenPolygonOwnerAccess{require_world(borrow)};
}
MutableScreenPolygonBaseReader live_screen_polygon_reader(const ScreenPolygonOwnerAccess &access)
{
    return MutableScreenPolygonBaseReader::live(
        &access,
        [](const void *context) -> std::span<SourceScreenPolygon> {
            return static_cast<const ScreenPolygonOwnerAccess *>(context)->published_records();
        },
        [](const void *context) { return static_cast<const ScreenPolygonOwnerAccess *>(context)->published_source(); });
}
ConstScreenPolygonBaseReader live_screen_polygon_const_reader(const ScreenPolygonOwnerAccess &access)
{
    return ConstScreenPolygonBaseReader::live(
        &access,
        [](const void *context) -> std::span<const SourceScreenPolygon> {
            return static_cast<const ScreenPolygonOwnerAccess *>(context)->published_records();
        },
        [](const void *context) { return static_cast<const ScreenPolygonOwnerAccess *>(context)->published_source(); });
}
SourceSceneScreenBaseResult reset_source_scene_screen_base_42e572(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                                                  SourceScreenBaseStoreObserver *observer)
{
    const auto platform = require_world(borrow);
    auto &state = platform->state();
    state.screen_base.owner_begin_ = false;
    if (observer)
        observer->stored(0x42e572, 0x61c5f0, state);
    return {};
}
SourceScreenBasePublicationResult publish_source_screen_base_42e4dc(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                                                    SourceScreenBaseStoreObserver *observer)
{
    const auto platform = require_world(borrow);
    auto &state = platform->state();
    if (state.screen_polygons.source_bound())
        throw FormatError("native screen publication cannot replace source pointer");
    state.screen_base.owner_begin_ = true;
    if (observer)
        observer->stored(0x42e4dc, 0x61c5f0, state);
    return {};
}
} // namespace sgn::retail
