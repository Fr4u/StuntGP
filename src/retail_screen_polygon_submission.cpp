#include "sgn/retail_screen_polygon_submission.hpp"
#include <bit>
#include <optional>
#include <cstring>
#include "sgn/source_screen_transport.hpp"
#include "sgn/format_require.hpp"

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
} // namespace
void apply_screen_polygon_blend_432190(std::uint32_t mode, PolygonSubmissionSink &sink)
{
    // Literal432288/4322A0 dispatch. This helper accepts ECX, not flags &78.
    const auto enable = (mode == 0x48 || mode == 0x50 || mode == 0x58 || mode == 0x60 || mode == 0x70) ? 1u : 0u;
    const auto source = mode == 0x70 ? 9u : mode == 0x58 ? 2u : enable ? 5u : 2u;
    const auto destination = (mode == 0x48 || mode == 0x50) ? 6u : (mode == 0x58 || mode == 0x60) ? 2u : 1u;
    sink.set_render_state(27, enable);
    sink.set_render_state(19, source);
    sink.set_render_state(20, destination);
    sink.set_texture_stage_state(0, 2, 2);
}
void reset_screen_polygon_count_425870(std::uint32_t &count)
{
    count = 0;
}
void reset_screen_material_cache_42e420(std::uint32_t &selected_material)
{
    selected_material = 0xffffffffu;
}
ScreenPolygonSubmissionResult submit_screen_polygons_432f60(ScreenPolygonSubmission frame, PolygonSubmissionSink &sink)
{
    sink.set_render_state(24, 1);            // 432F76; may mutate both source words
    const auto entry_count = frame.count;    // 432F79, never admitted before callback
    const auto source = frame.base.source(); // 432F7E, same captured allocation even after callbacks
    const auto slots = source ? std::span<const SourceScreenPolygon>{} : frame.base.read();
    std::optional<SourceTransportBuffer> source_bytes;
    if (source)
        source_bytes.emplace(*source);
    auto word = [&](std::uint32_t index, std::uint32_t at, std::uint32_t site) {
        if (source_bytes)
            return source_bytes->number(index * 272u + at, site);
        require(index < slots.size(), "screen submission polygon outside retained owner");
        std::uint32_t value;
        std::memcpy(&value, reinterpret_cast<const std::uint8_t *>(&slots[index]) + at, 4);
        return value;
    };
    std::uint32_t index{}, mode = 0xffffffffu;
    if (entry_count)
        do
        {
            const auto alpha = word(index, 8, 0x432f9c) << 24;
            const auto copied = word(index, 12, 0x432f9f);
            require(copied <= 8, "screen submission vertex count outside source local buffer");
            for (std::uint32_t i = 0; i < copied; ++i)
            {
                auto &out = frame.scratch[i].words;
                const auto at = 16u + i * 32u;
                out[0] = word(index, at, 0x432fb2);
                out[1] = word(index, at + 4, 0x432fba);
                out[2] = word(index, at + 8, 0x432fc0);
                out[3] = out[2];
                out[6] = word(index, at + 16, 0x432fc8);
                out[7] = word(index, at + 20, 0x432fce);
                out[4] = word(index, at + 24, 0x432fd4) | alpha;
                out[5] = 0xff000000u;
            }
            const auto next_mode = word(index, 4, 0x432fe9) & 0x78;
            if (mode != next_mode)
            {
                mode = next_mode;
                apply_screen_polygon_blend_432190(mode, sink);
            }
            const auto material = word(index, 0, 0x432ffe);
            if (frame.selected_material != material)
            {
                require(material < frame.materials.size(), "screen submission material outside live namespace");
                sink.set_texture(0, frame.materials[material].primary_texture);
                frame.selected_material = material; // 43302A, after SetTexture regardless of HRESULT
            }
            // 433030 rereads count after the state/texture API calls.
            const auto drawn = word(index, 12, 0x433030);
            require(drawn <= 8, "screen submission draw count outside source local buffer");
            sink.draw(6, 0x2c4, frame.scratch.first(drawn), 0x14, frame.viewport);
            ++index;
        } while (index < frame.count); // 433052 live count, not a captured entry count
    const auto tail =
        source_bytes ? std::uint32_t(source_bytes->read(index * 272u + 4, 1, 0x43306c)[0]) : word(index, 4, 0x43306c);
    if (tail & 0x40)
        sink.set_render_state(27, 0); // 43306C, next slot
    reset_screen_polygon_count_425870(frame.count);
    sink.set_render_state(24, 160);
    return {entry_count, index};
}
} // namespace sgn::retail
