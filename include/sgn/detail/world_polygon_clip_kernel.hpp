#pragma once
#include "sgn/retail_world_polygons.hpp"
#include "sgn/retail_projection.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace sgn::retail::detail
{
inline void clip_require(bool ok, const char *why)
{
    if (!ok)
        throw FormatError(why);
}
inline float clip_f(std::uint32_t word) noexcept
{
    return std::bit_cast<float>(word);
}
inline std::uint32_t clip_u(float value) noexcept
{
    return std::bit_cast<std::uint32_t>(value);
}
struct ClipOutputPublication
{
    SourceClipCurrentPointer header;
    std::uint32_t attribute_start{}, vertex_start{}; // native diagnostics only
};
struct Corner
{
    WorldPolygonAttribute attribute;
    WorldPolygonVertex vertex;
    std::array<std::uint8_t, 48> undefined{};
};
inline std::uint16_t inside(const Corner &c) noexcept
{
    return static_cast<std::uint16_t>(c.vertex.words[4]);
}
inline void check_corner(const Corner &c)
{
    for (unsigned i : {0u, 1u, 2u, 7u})
        clip_require(std::isfinite(clip_f(c.vertex.words[i])), "nonfinite source polygon vertex");
    clip_require(std::isfinite(clip_f(c.attribute.u)) && std::isfinite(clip_f(c.attribute.v)),
                 "nonfinite source polygon UV");
    for (unsigned i : {0u,  1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,  9u,  12u, 13u, 14u, 15u, 16u, 17u,
                       18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 32u, 33u, 44u, 45u, 46u, 47u})
        clip_require(c.undefined[i] == 0, "undefined source polygon input affects clipping");
}
inline void intersection(const Corner &first, const Corner &second, Corner &output, unsigned plane, float near_distance)
{
    const auto convert = [](const Corner &c) {
        const auto &v = c.vertex.words;
        return ClipCorner{clip_f(c.attribute.u),
                          clip_f(c.attribute.v),
                          c.attribute.color,
                          {clip_f(v[0]), clip_f(v[1]), clip_f(v[2]), clip_f(v[7]), static_cast<std::uint16_t>(v[4])}};
    };
    ClipCorner result;
    if (plane != 2)
    {
        result = retail_edge_intersection(convert(first), convert(second), plane, near_distance);
    }
    else
    {
        // 4272A0's default jump-table entry for plane2 loads the same plane
        // word twice, producing delta0 and t1. Source producers normally mark
        // bit2 inside, but the complete consumer retains this exact branch.
        const auto a = convert(first), b = convert(second);
        const auto mix = [](float x, float y) { return static_cast<float>((double(y) - x) * 1.0 + x); };
        result = {mix(a.u, b.u),
                  mix(a.v, b.v),
                  0,
                  {mix(a.point.x, b.point.x), mix(a.point.y, b.point.y), mix(a.point.z, b.point.z),
                   mix(a.point.camera_z, b.point.camera_z), 0}};
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            const auto left = (a.color >> shift) & 255u, right = (b.color >> shift) & 255u;
            const auto product = std::bit_cast<std::int32_t>((right - left) * 256u);
            result.color |= ((static_cast<std::uint32_t>(product >> 8) + left) & 255u) << shift;
        }
        result.point.inside =
            projection_mask(result.point.x, result.point.y, result.point.z, result.point.camera_z, near_distance);
    }
    // Only the source-written lanes change; all other scratch bytes retain
    // the preceding contents of this particular scratch slot.
    output.attribute.u = clip_u(result.u);
    output.attribute.v = clip_u(result.v);
    output.attribute.color = result.color;
    auto &v = output.vertex.words;
    v[0] = clip_u(result.point.x);
    v[1] = clip_u(result.point.y);
    v[2] = clip_u(result.point.z);
    v[7] = clip_u(result.point.camera_z);
    v[4] = (v[4] & 0xffff0000u) | result.point.inside;
    for (unsigned i : {0u,  1u,  2u,  3u,  4u,  5u,  6u,  7u,  12u, 13u, 14u, 15u, 16u, 17u, 18u,
                       19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 32u, 33u, 44u, 45u, 46u, 47u})
        output.undefined[i] = 0;
}
template <class Access>
void clip_polygon_kernel(Access &access, WorldPolygonClipStorage &storage,
                         const WorldPolygonPacketAudit *input_audit = nullptr,
                         WorldPolygonClipAudit *output_audit = nullptr)
{
    const auto original_header = access.capture_header(); // source captures indices427543 first
    clip_require(bool(original_header), "source polygon header is null");
    if constexpr (!Access::source)
        clip_require(std::isfinite(access.near_distance()), "nonfinite source clip near plane");
    unsigned count = original_header.byte(0);
    clip_require(count <= 16, "source polygon input exceeds sixteen scratch vertices");
    if constexpr (!Access::source)
        access.validate_count(count);
    if (input_audit)
    {
        clip_require(!input_audit->header || (*input_audit->header)[0] == 0, "undefined polygon vertex count");
        clip_require(input_audit->attributes.empty() || input_audit->attributes.size() == access.attribute_size(),
                     "polygon attribute audit extent");
        clip_require(input_audit->vertices.empty() || input_audit->vertices.size() == access.vertex_size(),
                     "polygon vertex audit extent");
    }
    if (output_audit)
        clip_require(output_audit->headers.size() == storage.headers.size() &&
                         output_audit->attributes.size() == storage.attributes.size() &&
                         output_audit->vertices.size() == storage.vertices.size(),
                     "clip storage audit extent");
    std::array<std::array<Corner, 16>, 2> scratch{};
    for (auto &bank : scratch)
        for (auto &corner : bank)
            corner.undefined.fill(1);
    unsigned active = 1; // source local328 receives the initial attribute/vertex copies
    for (unsigned i = 0; i < count; ++i)
    {
        const auto attribute = access.index(i);
        auto &dst = scratch[active][i];
        dst.attribute = access.attribute(attribute);
        dst.vertex = access.vertex(dst.attribute.vertex);
        dst.undefined.fill(0);
        if (input_audit)
        {
            if (!input_audit->attributes.empty())
                std::copy(input_audit->attributes[attribute].begin(), input_audit->attributes[attribute].end(),
                          dst.undefined.begin());
            if (!input_audit->vertices.empty())
                std::copy(input_audit->vertices[dst.attribute.vertex].begin(),
                          input_audit->vertices[dst.attribute.vertex].end(), dst.undefined.begin() + 16);
        }
        check_corner(dst);
    }
    auto common = access.mask();
    for (unsigned plane = 1; plane <= 32; plane <<= 1)
    {
        if ((common & plane) == 0)
        {
            const auto &input = scratch[active];
            auto &output = scratch[active ^ 1u];
            unsigned written = 0;
            for (unsigned i = 0; i < count; ++i)
            {
                const auto &previous = input[i ? i - 1 : count - 1];
                const auto &current = input[i];
                const bool previous_inside = (inside(previous) & plane) != 0,
                           current_inside = (inside(current) & plane) != 0;
                if (previous_inside)
                {
                    clip_require(written < 16, "source polygon clipping exceeds sixteen scratch vertices");
                    output[written++] = previous;
                }
                if (previous_inside != current_inside)
                {
                    clip_require(written < 16, "source polygon clipping exceeds sixteen scratch vertices");
                    intersection(previous, current, output[written++], plane, access.near_distance());
                }
            }
            count = written;
            if (count < 3)
            {
                access.reject();
                return;
            }
            active ^= 1u;
        }
        common |= plane;
        if (common == 63)
            break;
    }
    // Native admission stays in its adapter. The source adapter publishes
    // actual packet pointers before any output header/payload dereference.
    const auto output = access.publish_output(storage, count);
    const auto header_pointer = output.header;
    const auto header_start = header_pointer.count_from(storage.headers);
    const auto attribute_start = output.attribute_start, vertex_start = output.vertex_start;
    auto &destination_header = storage.headers[header_start];
    // Read each original DWORD only when reached. The original reference can
    // alias output, so copying all eight header bytes in advance is incorrect.
    for (unsigned lane = 0; lane < 2; ++lane)
    {
        std::array<std::uint8_t, 4> word{};
        for (unsigned byte = 0; byte < 4; ++byte)
            word[byte] = original_header.byte(lane * 4 + byte);
        std::memcpy(destination_header.data() + lane * 4, word.data(), 4); // 4276F5,4276FB
    }
    destination_header[0] = static_cast<std::uint8_t>(count);                       // 4276FE
    destination_header[6] = destination_header[7] = 0;                              // 427701 WORD0
    storage.header_count = header_pointer.template advanced<WorldPolygonHeader>(1); // 427705
    if (output_audit)
    {
        std::array<std::uint8_t, 8> undefined{};
        if (input_audit && input_audit->header)
            undefined = *input_audit->header;
        undefined[0] = undefined[6] = undefined[7] = 0;
        output_audit->headers[header_start] = undefined;
    }
    for (unsigned i = 0; i < count; ++i)
    {
        auto &corner = scratch[active][i];
        const auto current_attribute = storage.attribute_count;  // 427729
        corner.attribute.vertex = static_cast<std::uint16_t>(i); // 42772F
        corner.undefined[8] = corner.undefined[9] = 0;
        auto &attribute = storage.attributes[current_attribute.count_from(storage.attributes)];
        for (unsigned word = 0; word < 3; ++word)
            std::memcpy(reinterpret_cast<std::uint8_t *>(&attribute) + word * 4,
                        reinterpret_cast<const std::uint8_t *>(&corner.attribute) + word * 4, 4); // 42773A/3F/45
        const auto current_vertex = storage.vertex_count;                                         // 427748
        std::memcpy(reinterpret_cast<std::uint8_t *>(&attribute) + 12,
                    reinterpret_cast<const std::uint8_t *>(&corner.attribute) + 12, 4); // 427751
        auto &vertex = storage.vertices[current_vertex.count_from(storage.vertices)];
        for (unsigned word = 0; word < 8; ++word)
            vertex.words[word] = corner.vertex.words[word]; // 42775C REP MOVSD
        // Both pointer values are read before either global publication.
        const auto next_attribute = storage.attribute_count.advanced<WorldPolygonAttribute>(1); // 42775E/6A
        const auto next_vertex = storage.vertex_count.advanced<WorldPolygonVertex>(1);          // 427764/6D
        storage.attribute_count = next_attribute;                                               // 427770
        storage.vertex_count = next_vertex;                                                     // 42777F
        if (output_audit)
        {
            std::copy_n(corner.undefined.begin(), 16, output_audit->attributes[attribute_start + i].begin());
            std::copy_n(corner.undefined.begin() + 16, 32, output_audit->vertices[vertex_start + i].begin());
        }
    }
    // The source record's common mask is retained, including after replacement.
}
} // namespace sgn::retail::detail
