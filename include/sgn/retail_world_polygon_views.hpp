#pragma once
#include "sgn/retail_light_effects.hpp"
#include "sgn/retail_vehicle_strip.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>

namespace sgn::retail
{
using WorldPolygonHeader = std::array<std::uint8_t, 8>;
using WorldPolygonAttribute = SourceSpriteAttribute;
static_assert(sizeof(WorldPolygonHeader) == 8 && sizeof(WorldPolygonAttribute) == 16);
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(std::is_trivially_copyable_v<VehicleStripPolygon> && std::is_standard_layout_v<VehicleStripPolygon>);
static_assert(sizeof(VehicleStripPolygon) == 8 && offsetof(VehicleStripPolygon, vertex_count) == 0 &&
              offsetof(VehicleStripPolygon, texture) == 1 && offsetof(VehicleStripPolygon, field_02) == 2 &&
              offsetof(VehicleStripPolygon, flags) == 3 && offsetof(VehicleStripPolygon, normal) == 4 &&
              offsetof(VehicleStripPolygon, first_corner) == 6);
static_assert(std::is_trivially_copyable_v<VehicleStripCorner> && std::is_standard_layout_v<VehicleStripCorner>);
static_assert(sizeof(VehicleStripCorner) == 16 && offsetof(VehicleStripCorner, u) == 0 &&
              offsetof(VehicleStripCorner, v) == 4 && offsetof(VehicleStripCorner, vertex) == 8 &&
              offsetof(VehicleStripCorner, field_0a) == 10 && offsetof(VehicleStripCorner, color) == 12);
static_assert(std::is_trivially_copyable_v<WorldPolygonAttribute> && std::is_standard_layout_v<WorldPolygonAttribute>);
static_assert(offsetof(WorldPolygonAttribute, u) == 0 && offsetof(WorldPolygonAttribute, v) == 4 &&
              offsetof(WorldPolygonAttribute, vertex) == 8 && offsetof(WorldPolygonAttribute, retained) == 10 &&
              offsetof(WorldPolygonAttribute, color) == 12);

template <class T>
concept WorldPolygonHeaderRecord = std::is_same_v<std::remove_const_t<T>, WorldPolygonHeader> ||
                                   std::is_same_v<std::remove_const_t<T>, VehicleStripPolygon>;

class WorldPolygonHeaderView;

// One borrowed identity, never a pointer to a fabricated array object. All
// reads observe the current owner bytes; value() alone creates a short copy.
class WorldPolygonHeaderRef
{
    const std::byte *address_{};
    explicit WorldPolygonHeaderRef(const std::byte *address) noexcept : address_(address) {}
    friend class WorldPolygonHeaderView;

  public:
    WorldPolygonHeaderRef() = default;
    explicit operator bool() const noexcept
    {
        return address_ != nullptr;
    }
    const void *identity() const noexcept
    {
        return address_;
    }
    std::uint8_t byte(std::size_t index) const
    {
        if (!address_ || index >= 8)
            throw FormatError("source polygon header reference index");
        return std::to_integer<std::uint8_t>(address_[index]);
    }
    WorldPolygonHeader value() const
    {
        if (!address_)
            throw FormatError("source polygon header reference is null");
        WorldPolygonHeader result;
        std::memcpy(result.data(), address_, sizeof(result));
        return result;
    }
    friend bool operator==(WorldPolygonHeaderRef, WorldPolygonHeaderRef) = default;
};
static_assert(sizeof(WorldPolygonHeaderRef) == sizeof(const WorldPolygonHeader *));

class WorldPolygonHeaderView
{
    std::span<const std::byte> bytes_;
    explicit WorldPolygonHeaderView(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

  public:
    WorldPolygonHeaderView() = default;
    template <WorldPolygonHeaderRecord T, std::size_t N>
    WorldPolygonHeaderView(std::span<T, N> records) noexcept : bytes_(std::as_bytes(records))
    {
    }
    std::size_t size() const noexcept
    {
        return bytes_.size() / 8;
    }
    bool empty() const noexcept
    {
        return bytes_.empty();
    }
    std::span<const std::byte> bytes() const noexcept
    {
        return bytes_;
    }
    const void *identity() const noexcept
    {
        return bytes_.data();
    }
    WorldPolygonHeaderRef at(std::size_t index) const
    {
        if (index >= size())
            throw FormatError("source polygon header view index");
        return WorldPolygonHeaderRef(bytes_.data() + index * 8);
    }
    WorldPolygonHeaderView subspan(std::size_t first, std::size_t count = std::dynamic_extent) const
    {
        if (first > size())
            throw FormatError("source polygon header subspan start");
        if (count == std::dynamic_extent)
            count = size() - first;
        if (count > size() - first)
            throw FormatError("source polygon header subspan count");
        return WorldPolygonHeaderView(bytes_.subspan(first * 8, count * 8));
    }
    std::optional<std::size_t> find(WorldPolygonHeaderRef ref) const noexcept
    {
        if (!ref)
            return std::nullopt;
        for (std::size_t i = 0; i < size(); ++i)
            if (ref.identity() == bytes_.data() + i * 8)
                return i;
        return std::nullopt;
    }
    std::size_t index_of(WorldPolygonHeaderRef ref) const
    {
        const auto result = find(ref);
        if (!result)
            throw FormatError("source polygon header has a different owner");
        return *result;
    }
};

// Only a known lvalue record can construct this one-record borrowed view.
template <WorldPolygonHeaderRecord T> WorldPolygonHeaderRef world_polygon_header_ref(T &record) noexcept
{
    return WorldPolygonHeaderView(std::span(&record, 1)).at(0);
}
template <WorldPolygonHeaderRecord T> WorldPolygonHeaderRef world_polygon_header_ref(const T &&) = delete;

class WorldPolygonAttributeView
{
    std::span<const std::byte> bytes_;

  public:
    WorldPolygonAttributeView() = default;
    template <class T, std::size_t N>
        requires(std::is_same_v<std::remove_const_t<T>, WorldPolygonAttribute> ||
                 std::is_same_v<std::remove_const_t<T>, std::array<std::uint32_t, 4>> ||
                 std::is_same_v<std::remove_const_t<T>, VehicleStripCorner>)
    WorldPolygonAttributeView(std::span<T, N> values) noexcept : bytes_(std::as_bytes(values))
    {
    }
    std::size_t size() const noexcept
    {
        return bytes_.size() / sizeof(WorldPolygonAttribute);
    }
    std::span<const std::byte> bytes() const noexcept
    {
        return bytes_;
    }
    WorldPolygonAttribute value(std::size_t index) const
    {
        if (index >= size())
            throw FormatError("source polygon attribute view index");
        WorldPolygonAttribute result;
        std::memcpy(&result, bytes_.data() + index * sizeof(result), sizeof(result));
        return result;
    }
};
} // namespace sgn::retail
