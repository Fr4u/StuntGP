#pragma once
#include "sgn/retail_viewport.hpp"
#include <cstddef>
#include "sgn/source_world_span.hpp"
#include <optional>
#include <span>
#include <stdexcept>

namespace sgn::retail
{
struct SourceScreenVertex
{
    float x{}, y{}, z{};
    std::uint32_t retained_w{};
    float u{}, v{};
    std::uint32_t color{}, outcode{};
};
struct SourceScreenPolygon
{
    std::uint32_t texture{}, flags{}, alpha{}, vertex_count{};
    std::array<SourceScreenVertex, 8> vertices{};
};
static_assert(sizeof(SourceScreenVertex) == 32 && sizeof(SourceScreenPolygon) == 272);
// Test-only source provenance. No undefined lane may control clipping.
struct ScreenPolygonAudit
{
    std::array<bool, 68> undefined{};
};
// A synchronous nonowning read, invoked only at the decoded source load.
// Live context lifetime belongs to the caller. No implicit span conversion,
// cached world base, allocation, or default NULL-to-live substitution.
template <class Polygon> class ScreenPolygonBaseReader
{
  public:
    using SourceRead = std::optional<SourceWorldBufferBinding> (*)(const void *);
    using Read = std::span<Polygon> (*)(const void *);
    static ScreenPolygonBaseReader live(const void *context, Read read, SourceRead source = nullptr)
    {
        if (!context || !read)
            throw std::invalid_argument("screen base reader context");
        return ScreenPolygonBaseReader{context, read, {}, source};
    }
    // Explicit fixture contract: caller already owns this fixed buffer.
    static ScreenPolygonBaseReader fixed_fixture(std::span<Polygon> slots)
    {
        return ScreenPolygonBaseReader{nullptr, nullptr, slots};
    }
    std::optional<SourceWorldBufferBinding> source() const
    {
        return source_ ? source_(context_) : std::nullopt;
    }
    std::span<Polygon> read() const
    {
        return read_ ? read_(context_) : fixed_;
    }

  private:
    const void *context_;
    Read read_;
    SourceRead source_;
    std::span<Polygon> fixed_;
    ScreenPolygonBaseReader(const void *context, Read read, std::span<Polygon> fixed, SourceRead source = nullptr)
        : context_(context), read_(read), source_(source), fixed_(fixed)
    {
    }
};
using MutableScreenPolygonBaseReader = ScreenPolygonBaseReader<SourceScreenPolygon>;
using ConstScreenPolygonBaseReader = ScreenPolygonBaseReader<const SourceScreenPolygon>;
struct SourceScreenPolygonQueue
{
    MutableScreenPolygonBaseReader base; // 61C5F0 load on EACH425838
    std::uint32_t &count;                // source61C604; reset belongs to425870/caller
};
// Full4253E0: source threshold1, truncating256-color interpolation,
// strict right/bottom intersection outcodes, destination W unchanged.
void intersect_screen_polygon_edge(const SourceScreenVertex &, const SourceScreenVertex &, SourceScreenVertex &,
                                   std::uint32_t plane, const ViewportState &);
// Full4255E0. A rejected polygon retains any outcodes already written.
bool clip_screen_polygon(SourceScreenPolygon &, const ViewportState &, ScreenPolygonAudit *audit = nullptr);
// Full425830. Copies the whole272-byte record before clipping. A rejected
// candidate remains in slots[count], while count itself does not advance.
bool append_screen_polygon(SourceScreenPolygonQueue, const SourceScreenPolygon &, const ViewportState &,
                           const ScreenPolygonAudit *input_audit = nullptr,
                           std::span<ScreenPolygonAudit> queue_audit = {});
} // namespace sgn::retail
