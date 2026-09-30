#include "sgn/retail_screen_polygons.hpp"
#include "sgn/source_screen_transport.hpp"
#include <cstring>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using Tags = std::array<bool, 8>;
struct WorkingVertex
{
    SourceScreenVertex value;
    Tags tags;
};
void viewport(const ViewportState &view)
{
    for (float value : view.clip_screen)
        if (!std::isfinite(value))
            throw std::invalid_argument("screen clip bounds");
    for (float value : view.clip_expanded)
        if (!std::isfinite(value))
            throw std::invalid_argument("screen guard bounds");
    for (unsigned j = 0; j < 4; j += 2)
        if (view.clip_screen[j] > view.clip_screen[j + 1] || view.clip_expanded[j] > view.clip_screen[j] ||
            view.clip_expanded[j + 1] < view.clip_screen[j + 1])
            throw std::invalid_argument("screen clip bounds order");
}
void vertex(const SourceScreenVertex &v)
{
    for (float value : {v.x, v.y, v.z, v.u, v.v})
        if (!std::isfinite(value))
            throw std::invalid_argument("nonfinite screen vertex");
}
void validate(const SourceScreenPolygon &p, const ViewportState &view, const ScreenPolygonAudit *audit)
{
    if (p.vertex_count > 8)
        throw std::invalid_argument("screen polygon vertex count");
    viewport(view);
    if (audit && audit->undefined[3])
        throw std::invalid_argument("undefined screen vertex count");
    for (unsigned j = 0; j < p.vertex_count; ++j)
    {
        vertex(p.vertices[j]);
        if (audit)
            for (unsigned k : {0u, 1u, 2u, 4u, 5u, 6u})
                if (audit->undefined[4 + j * 8 + k])
                    throw std::invalid_argument("undefined screen clip input");
    }
}
std::uint32_t outcode(float x, float y, const ViewportState &view, bool intersection)
{
    std::uint32_t code = 15;
    if (x < view.clip_screen[0])
        code = 14;
    else if (intersection ? x > view.clip_screen[1] : x >= view.clip_screen[1])
        code = 13;
    if (y < view.clip_screen[2])
        code &= ~4u;
    else if (intersection ? y > view.clip_screen[3] : y >= view.clip_screen[3])
        code &= ~8u;
    return code;
}
void intersect(const SourceScreenVertex &first, const SourceScreenVertex &second, SourceScreenVertex &output,
               std::uint32_t plane, const ViewportState &view)
{
    const unsigned axis = plane == 1 ? 0 : plane == 2 ? 1 : plane == 4 ? 2 : 3;
    const double a = double(axis < 2 ? first.x : first.y) - view.clip_screen[axis];
    const double b = double(axis < 2 ? second.x : second.y) - view.clip_screen[axis];
    const double delta = b - a;
    const double t = std::abs(delta) > 1 ? -(a / delta) : 1;
    const auto blend = [&](float x, float y) { return static_cast<float>((double(y) - x) * t + x); };
    output.x = blend(first.x, second.x);
    output.y = blend(first.y, second.y);
    output.z = blend(first.z, second.z);
    output.u = blend(first.u, second.u);
    output.v = blend(first.v, second.v);
    const auto factor = static_cast<std::uint32_t>(static_cast<std::int64_t>(t * 256));
    std::uint32_t color = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
    {
        const auto left = (first.color >> shift) & 255u, right = (second.color >> shift) & 255u;
        const auto product = std::bit_cast<std::int32_t>((right - left) * factor);
        color |= (((static_cast<std::uint32_t>(product >> 8) + left) & 255u) << shift);
    }
    output.color = color;
    output.outcode = outcode(output.x, output.y, view, true);
}
bool clip(SourceScreenPolygon &p, const ViewportState &view, ScreenPolygonAudit *audit)
{
    std::uint32_t both = 15, either = 0;
    for (unsigned j = 0; j < p.vertex_count; ++j)
    {
        auto &v = p.vertices[j];
        if (v.x < view.clip_expanded[0] || v.x > view.clip_expanded[1] || v.y < view.clip_expanded[2] ||
            v.y > view.clip_expanded[3])
            return false;
        v.outcode = outcode(v.x, v.y, view, false);
        if (audit)
            audit->undefined[4 + j * 8 + 7] = false;
        both &= v.outcode;
        either |= v.outcode;
    }
    if (both == 15)
        return true;
    if (either != 15)
        return false;
    std::array<std::array<WorkingVertex, 16>, 2> scratch{};
    for (auto &buffer : scratch)
        for (auto &v : buffer)
            v.tags.fill(true);
    for (unsigned j = 0; j < p.vertex_count; ++j)
    {
        scratch[0][j].value = p.vertices[j];
        scratch[0][j].tags.fill(false);
        if (audit)
            for (unsigned k = 0; k < 8; ++k)
                scratch[0][j].tags[k] = audit->undefined[4 + j * 8 + k];
    }
    unsigned active = 0, count = p.vertex_count;
    for (std::uint32_t plane = 1; plane <= 8; plane <<= 1)
    {
        if (both & plane)
            continue;
        auto &input = scratch[active];
        auto &output = scratch[active ^ 1u];
        unsigned written = 0;
        for (unsigned j = 0; j < count; ++j)
        {
            const auto &prev = input[j ? j - 1 : count - 1];
            const auto &next = input[j];
            const bool pi = (prev.value.outcode & plane) != 0, ni = (next.value.outcode & plane) != 0;
            if (pi)
                output[written++] = prev;
            if (pi != ni)
            {
                auto &dst = output[written++];
                intersect(prev.value, next.value, dst.value, plane, view);
                for (unsigned k : {0u, 1u, 2u, 4u, 5u, 6u, 7u})
                    dst.tags[k] = false; // W retains that scratch slot's provenance
            }
        }
        if (written < 3 || written > 6)
            return false;
        active ^= 1u;
        count = written;
    }
    for (unsigned j = 0; j < count; ++j)
    {
        const auto &v = scratch[active][j];
        auto words = std::bit_cast<std::array<std::uint32_t, 8>>(v.value);
        const auto retained = std::bit_cast<std::array<std::uint32_t, 8>>(p.vertices[j]);
        for (unsigned k = 0; k < 8; ++k)
        {
            if (v.tags[k])
                words[k] = retained[k];
            if (audit)
                audit->undefined[4 + j * 8 + k] = v.tags[k];
        }
        p.vertices[j] = std::bit_cast<SourceScreenVertex>(words);
    }
    p.vertex_count = count;
    if (audit)
        audit->undefined[3] = false;
    return true;
}
} // namespace
void intersect_screen_polygon_edge(const SourceScreenVertex &first, const SourceScreenVertex &second,
                                   SourceScreenVertex &output, std::uint32_t plane, const ViewportState &view)
{
    if (plane != 1 && plane != 2 && plane != 4 && plane != 8)
        throw std::invalid_argument("screen clip plane");
    viewport(view);
    vertex(first);
    vertex(second);
    // Public helper permits finite noncrossing edges. Guard the source FTOL
    // domain before touching destination; crossing paths stay within[0,256].
    const unsigned axis = plane == 1 ? 0 : plane == 2 ? 1 : plane == 4 ? 2 : 3;
    const double a = double(axis < 2 ? first.x : first.y) - view.clip_screen[axis];
    const double d = (double(axis < 2 ? second.x : second.y) - view.clip_screen[axis]) - a;
    const double factor = (std::abs(d) > 1 ? -(a / d) : 1) * 256;
    if (!std::isfinite(factor) || factor < -9223372036854775808.0 || factor >= 9223372036854775808.0)
        throw std::invalid_argument("screen edge integer range");
    intersect(first, second, output, plane, view);
}
bool clip_screen_polygon(SourceScreenPolygon &p, const ViewportState &view, ScreenPolygonAudit *audit)
{
    validate(p, view, audit);
    return clip(p, view, audit);
}
bool append_screen_polygon(SourceScreenPolygonQueue queue, const SourceScreenPolygon &input, const ViewportState &view,
                           const ScreenPolygonAudit *input_audit, std::span<ScreenPolygonAudit> queue_audit)
{
    const auto index = queue.count; // 425830, before the base read
    if (const auto binding = queue.base.source())
    {
        return append_source_screen_captured_425838(
            *binding, index, queue.count,
            [&](std::uint32_t off, std::uint32_t site) {
                if (off > sizeof(input) - 4)
                    throw SourceAllocationError(site, "screen input copy outside272-byte owner");
                if (input_audit && input_audit->undefined[off / 4])
                    return SourceTransportWord::opaque(0x425830, site, static_cast<std::int32_t>(off));
                std::uint32_t bits;
                std::memcpy(&bits, reinterpret_cast<const std::uint8_t *>(&input) + off, 4);
                return SourceTransportWord::known(bits);
            },
            view);
    }
    const auto slots = queue.base.read(); // 425838, captured only for this append
    if (index >= slots.size())
        throw std::invalid_argument("screen polygon queue capacity");
    if (!queue_audit.empty() && queue_audit.size() != slots.size())
        throw std::invalid_argument("screen polygon queue audit capacity");
    validate(input, view, input_audit);
    slots[index] = input;
    ScreenPolygonAudit local = input_audit ? *input_audit : ScreenPolygonAudit{};
    auto *audit = queue_audit.empty() ? &local : &queue_audit[index];
    *audit = local;
    const bool accepted = clip(slots[index], view, audit);
    if (accepted)
        ++queue.count;
    return accepted;
}
} // namespace sgn::retail
