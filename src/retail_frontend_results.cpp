#include "sgn/retail_frontend_results.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint32_t get(std::span<const std::uint8_t> b, std::size_t at)
{
    if (at > b.size() || b.size() - at < 4)
        throw FormatError("frontend result source read outside owner");
    std::uint32_t value;
    std::memcpy(&value, b.data() + at, 4);
    return value;
}
void put(std::span<std::uint8_t> b, std::size_t at, std::uint32_t value)
{
    if (at > b.size() || b.size() - at < 4)
        throw FormatError("frontend result destination outside owner");
    std::memcpy(b.data() + at, &value, 4);
}
std::uint32_t clamp_percent(std::uint32_t value)
{
    // Full43D790's signed min/max selection for EDX0 and stack maximum100.
    return static_cast<std::uint32_t>(std::clamp(std::bit_cast<std::int32_t>(value), 0, 100));
}
void race_rows(FrontendRaceReturnWorld &w, const FrontendReturnBuffers &b, std::uint32_t count, std::size_t first,
               std::size_t stride)
{
    for (unsigned i = 0; i < count; ++i)
    {
        const auto grid = get(b.roster, first + i * stride);
        if (grid < 1 || grid > 6)
            throw FormatError("frontend source grid index is outside physical selection");
        const auto &previous = w.selection[grid - 1].previous_race;
        const auto row = 4 + std::size_t(i) * 20;
        // Preserve the source's actual write order; aborted is reread each row.
        put(b.result, row + 4, previous.best_lap_ticks);
        put(b.result, row + 8, std::bit_cast<std::uint32_t>(previous.race_timer));
        put(b.result, row + 12, previous.total_ticks);
        put(b.result, row + 16, previous.stunt_score);
        put(b.result, row, previous.finish_state_plus_one);
        const auto placing = std::bit_cast<std::int32_t>(previous.finish_state_plus_one);
        if (get(b.result, 0) == 0 && (placing < 1 || placing > 6))
            put(b.result, row, 6);
    }
}
} // namespace
std::uint32_t frontend_replay_identity(std::optional<ReplayHandle> handle)
{
    if (!handle)
        return 0;
    if (handle->slot >= ReplayBank::slots)
        throw FormatError("frontend recording identity outside arena");
    return handle->slot + 1;
}
std::optional<ReplayHandle> frontend_replay_handle(std::uint32_t identity)
{
    if (identity == 0)
        return std::nullopt;
    if (identity > ReplayBank::slots)
        throw FormatError("frontend recording identity outside arena");
    return ReplayHandle{identity - 1};
}
void return_frontend_race_result(FrontendRaceReturnWorld &w)
{
    // 439E90..439F2C. These words alias the one persistent frontend bank.
    put(w.persistence, 0x84, w.options.render_a);
    put(w.persistence, 0x80, w.options.audio_b);
    put(w.persistence, 0x7c, w.options.audio_a);
    put(w.persistence, 0x7c, clamp_percent(get(w.persistence, 0x7c)));
    put(w.persistence, 0x80, clamp_percent(get(w.persistence, 0x80)));
    const auto render = clamp_percent(get(w.persistence, 0x84));
    const auto sound = get(w.persistence, 0x80);
    put(w.persistence, 0x80, sound - sound % 10);
    const auto music = get(w.persistence, 0x7c);
    put(w.persistence, 0x7c, music - music % 10);
    put(w.persistence, 0x70, w.options.display_selector);
    put(w.persistence, 0x84, render - render % 10);
    if (w.selector > 0 && w.selector < 8 && w.selector != 2)
    {
        const auto &b = w.buffers[w.selector];
        put(b.result, 0, w.options.transient == 1 ? 1u : 0u);
        switch (w.selector)
        {
        case 1:
            race_rows(w, b, 6, 0xc, 0x1c);
            break;
        case 3:
            put(b.result, 0x10, w.selection[0].previous_race.best_lap_ticks);
            put(b.result, 4, w.trial.previous_completed);
            put(b.result, 8, frontend_replay_identity(w.trial.selected.replay));
            put(b.result, 0xc, w.trial.selected.header_value);
            break;
        case 4:
            put(b.result, 4, w.selection[0].previous_race.stunt_score);
            break;
        case 5:
            race_rows(w, b, 6, 0x138, 0x1c);
            break;
        case 6: {
            const auto count = get(b.roster, 0x30);
            put(b.result, 0x54, count);
            if (std::bit_cast<std::int32_t>(count) > 0)
            {
                if (count > 4)
                    throw FormatError("frontend mode6 source roster exceeds four entries");
                race_rows(w, b, count, 8, 0xc);
            }
            break;
        }
        case 7: {
            const auto count = get(b.roster, 0xc0);
            put(b.result, 0x7c, count);
            if (std::bit_cast<std::int32_t>(count) > 0)
            {
                if (count > 6)
                    throw FormatError("frontend mode7 source roster exceeds six entries");
                race_rows(w, b, count, 4, 0x20);
            }
            break;
        }
        default:
            break;
        }
    }
    w.selector = 0; // Every original return clears5DF528, including invalid slots.
}
} // namespace sgn::retail
