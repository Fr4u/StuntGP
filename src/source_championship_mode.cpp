#include "sgn/source_championship_mode.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/source_selection_pages.hpp"
#include "sgn/retail_championship.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
W read(FrontendState &f, W address, W width = 4)
{
    const auto b = f.source.source_bytes(address, width);
    W value = 0;
    for (W i = 0; i < width; ++i)
        value |= W(b[i]) << (i * 8u);
    return value;
}
void put(FrontendState &f, W address, W value, W width = 4)
{
    const auto b = f.source.source_bytes(address, width);
    for (W i = 0; i < width; ++i)
        b[i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
void copy_word(FrontendState &f, W source, W destination)
{
    const auto value = read(f, source);
    put(f, destination, value);
}
W stage_track_437920(FrontendState &f, W stage)
{
    return read(f, 0x5f9c10u + stage * 0x84u);
}
void unlock_car(FrontendState &f, W car)
{
    const auto b = std::as_writable_bytes(f.source.source_bytes(0x5df568u + car, 1));
    unlock_campaign_car_437880(std::span<std::byte, 1>{b.data(), 1});
}
void unlock_track(FrontendState &f, W track)
{
    const auto b = std::as_writable_bytes(f.source.source_bytes(0x5df580u + track, 1));
    unlock_campaign_track_4378a0(std::span<std::byte, 1>{b.data(), 1});
}
void class_unlock(FrontendState &f, W stage)
{
    const auto b = std::as_writable_bytes(f.source.source_bytes(0x5df568, 52));
    unlock_campaign_class_car_4378c0(std::span<std::byte, 52>{b.data(), 52}, stage);
}
// Full437790 at this call's real100-row table. Strict signed comparison,
// backward row displacement and original flag/value/WORD/BYTE store order.
void insert_score_437790(FrontendState &f, W initials, W value)
{
    W row = 0;
    for (; row < 100u; ++row)
        if (as_signed(read(f, 0x5df8d4u + row * 8u)) < as_signed(value))
            break;
    if (row == 100u)
        return; // EAX=-1 is discarded by44BB51
    for (W last = 99; last > row; --last)
    {
        const auto at = 0x5df8d4u + last * 8u;
        copy_word(f, at - 8u, at);
        copy_word(f, at - 4u, at + 4u);
    }
    const auto at = 0x5df8d4u + row * 8u;
    put(f, at + 7u, 1, 1);
    put(f, at, value);
    const auto first = read(f, initials, 2);
    put(f, at + 4u, first, 2);
    const auto third = read(f, initials + 2u, 1);
    put(f, at + 6u, third, 1);
}
void copy_initials_to_selection_43d690(FrontendState &f, W source, SelectionRecord &selected)
{
    // Capture WORD and BYTE before the source scratch stores; fourth byte stays.
    const auto first = read(f, source, 2), third = read(f, source + 2u, 1);
    auto &scratch = f.source.initials_scratch;
    scratch[0] = static_cast<std::uint8_t>(first);
    scratch[1] = static_cast<std::uint8_t>(first >> 8u);
    scratch[2] = static_cast<std::uint8_t>(third);
    const auto end = std::find(scratch.begin(), scratch.end(), std::uint8_t{});
    if (end == scratch.end())
        throw FormatError("43A39D initials scan leaves actual47966C owner");
    const auto length = static_cast<std::size_t>(end - scratch.begin()) + 1u;
    std::size_t copied = 0;
    for (; copied + 4u <= length; copied += 4u)
    {
        W value;
        std::memcpy(&value, scratch.data() + copied, 4);
        std::memcpy(selected.name.data() + copied, &value, 4);
    }
    for (; copied < length; ++copied)
        std::memcpy(selected.name.data() + copied, scratch.data() + copied, 1);
}
void prepare_championship_race_43a2a0(FrontendState &f)
{
    // Sole source call44B8F8 passes these actual static input/result owners.
    // Keep43A2B0,2B6,2BC publication order, before43A720 clears world selection.
    f.source.race_return_bindings[1].roster = f.source.source_bytes(0x61b880, 0xc0);
    f.source.race_return_selector = 1;
    f.source.race_return_bindings[1].result = f.source.source_bytes(0x61b940, 0x7c);
    // Both43D420 calls are the complete XOR EAX,EAX;RET source function.
    auto view = frontend_launch_world(f);
    clear_frontend_race_selection(view);
    auto &world = f.world();
    auto &o = world.options;
    auto &launch = world.frontend_launch;
    auto track = read(f, 0x61b928); // 43A2CC
    o.render_b = 1;
    o.game_type = 2;
    o.race_mode = 1;
    o.start_delay = 4;
    const auto table = track * 24u;
    --track;
    launch.prepared = 1;
    const auto music = read(f, 0x5fcb78u + table), texture = read(f, 0x5fcb7cu + table);
    launch.track_index = track;
    const auto laps = read(f, 0x61b92c);
    o.music = music;
    const auto record = read(f, 0x61b930);
    o.texture = texture;
    o.laps = laps;
    const auto record8 = read(f, 0x61b938);
    o.split = 0;
    o.demo_override = 0;
    const auto record4 = read(f, 0x61b934);
    o.record = record;
    const auto record12 = read(f, 0x61b93c);
    o.record_f008 = record8;
    o.record_f004 = record4;
    o.record_f00c = record12;
    W at = 0x61b884;
    for (W remaining = 6; remaining != 0; --remaining, at += 28u)
    {
        const auto grid = read(f, at + 8u) - 1u;
        if (grid >= world.selection.size())
            throw FormatError("43A38B grid leaves six actual selection owners");
        auto &selected = world.selection[grid];
        copy_initials_to_selection_43d690(f, at, selected);
        selected.kind = 2;
        if (read(f, at - 4u) != 0)
        {
            const auto controller = read(f, 0x5e37a4);
            selected.control = 0;
            bind_frontend_race_input(launch, controller, grid, 1);
            world.trial_selection.split_selection = grid;
        }
        else
        {
            const auto skill = read(f, at + 20u), mode = read(f, at + 16u);
            selected.control = 1;
            selected.ai_skill = skill;
            selected.ai_mode = mode;
        }
        const auto livery = read(f, at + 12u), model = read(f, at + 4u);
        prepare_frontend_stock_configuration_43a430(f, selected, model, livery);
    }
    // No scene tick or launch shortcut: caller creates the real Start25 page.
}
} // namespace
void advance_frontend_championship_mode_44b5e0(FrontendState &f, SourcePageFactoryServices &factory,
                                               SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    auto push = [&](W key, W kind) { (void)push_frontend_page_43dfa0(f, key, kind, factory, pages, music); };
    switch (read(f, 0x5e3794))
    {
    case 0:
        put(f, 0x5e379c, 1);
        put(f, 0x5e3794, 1);
        put(f, 0x5e3798, 0);
        put(f, 0x5e39e8, 0);
        put(f, 0x5e39e4, 0);
        put(f, 0x5e37ac, 0);
        put(f, 0x5e37a8, 0);
        put(f, 0x5e37b0, 6);
        put(f, 0x5e3954, 0);
        return;
    case 1:
        put(f, 0x5e3794, 2);
        push(8, 9);
        return;
    case 2:
        if (read(f, 0x5e3a30) != 0)
        {
            (void)pop_frontend_page_43eef0(f, pages, music);
            return;
        }
        put(f, 0x5e38f8, 0);
        for (W car = 1; car < 21u; ++car)
        {
            const auto available = source_car_available_437230(f, car, read(f, 0x5e3790));
            if (available != 0 || read(f, 0x5df5d4) != 0)
            {
                const auto count = read(f, 0x5e38f8);
                put(f, 0x5e38fcu + count * 4u, car);
                put(f, 0x5e38f8, read(f, 0x5e38f8) + 1u);
            }
        }
        put(f, 0x5e3794, 3);
        push(9, 10);
        return;
    case 3: {
        if (read(f, 0x5e38f4) != 0)
        {
            put(f, 0x5e3794, 1);
            return;
        }
        const auto selected = read(f, 0x5e3798);
        put(f, 0x5e39ec, 1);
        switch (source_car_class_437cc0(read(f, 0x5e3954u + selected * 4u)))
        {
        case 1:
            put(f, 0x5e39f0, 16);
            break;
        case 2:
        case 4:
            put(f, 0x5e39f0, 17);
            break;
        case 3:
            put(f, 0x5e39f0, 18);
            break;
        default:
            break;
        }
        const auto stage = read(f, 0x5e39f0);
        put(f, 0x5e3968, 0);
        const auto track = stage_track_437920(f, stage);
        const auto count = read(f, 0x5e3968);
        put(f, 0x5e396cu + count * 4u, track);
        put(f, 0x5e3968, read(f, 0x5e3968) + 1u);
        push(10, 11);
        put(f, 0x5e3794, 4);
        return; // phase AFTER factory callback
    }
    case 4: {
        if (read(f, 0x5e3964) != 0)
        {
            put(f, 0x5e3794, 2);
            return;
        }
        if (read(f, 0x5e39e4) != 0)
        {
            put(f, 0x5e3794, 8);
            return;
        }
        const auto stage_index = read(f, 0x5e39ec);
        put(f, 0x61b880, 1);
        const auto stage = read(f, 0x5e39ecu + stage_index * 4u);
        const auto laps = read(f, 0x5f9c0cu + stage * 0x84u);
        W source = 0x5f9c24u + stage * 0x84u;
        const auto selected = read(f, 0x5e3798), model = read(f, 0x5e3954u + selected * 4u);
        const auto initials = 0x5e3a34u + selected * 64u;
        put(f, 0x61b888, model);
        const auto first = read(f, initials, 2);
        put(f, 0x61b884, first, 2);
        const auto third = read(f, initials + 2u, 1);
        put(f, 0x61b886, third, 1);
        auto excluded = read(f, 0x5e37b0);
        put(f, 0x61b88c, excluded);
        put(f, 0x61b890, read(f, 0x5f26b4u + model * 0xecu));
        W grid = 1, destination = 0x61b89c;
        for (W remaining = 5; remaining != 0; --remaining)
        {
            if (grid == excluded)
                ++grid;
            const auto name = read(f, source - 12u, 2);
            put(f, destination + 4u, name, 2);
            const auto tail = read(f, source - 10u, 1);
            put(f, destination + 6u, tail, 1);
            const auto livery = read(f, source);
            put(f, destination, 0);
            put(f, destination + 16u, livery);
            const auto car = read(f, source - 4u);
            put(f, destination + 12u, grid);
            put(f, destination + 8u, car);
            copy_word(f, source + 4u, destination + 20u);
            copy_word(f, source + 8u, destination + 24u);
            excluded = read(f, 0x5e37b0);
            ++grid;
            if (grid == excluded)
                ++grid;
            source += 24u;
            destination += 28u;
        }
        put(f, 0x61b928, stage_track_437920(f, stage));
        put(f, 0x61b92c, laps);
        const auto at = stage * 32u;
        auto ecx = read(f, 0x5df674u + at), edx = read(f, 0x5df67cu + at);
        put(f, 0x61b930, ecx);
        ecx = read(f, 0x5df684u + at);
        put(f, 0x61b934, edx);
        edx = read(f, 0x5df68cu + at);
        put(f, 0x61b938, ecx);
        put(f, 0x61b93c, edx);
        prepare_championship_race_43a2a0(f);
        put(f, 0x5e3794, 5);
        push(0, 25);
        return;
    }
    case 5: {
        const auto stage = read(f, 0x5e39ecu + read(f, 0x5e39ec) * 4u);
        (void)stage_track_437920(f, stage); // read retained despite discarded EAX
        if (read(f, 0x61b940) != 0 || read(f, 0x61b944) == 0)
        {
            put(f, 0x5e3794, 8);
            return;
        }
        put(f, 0x5fa5d8, stage);
        for (W i = 0; i < 48u; ++i)
            copy_word(f, 0x61b880u + i * 4u, 0x5fa5dcu + i * 4u);
        for (W i = 0; i < 31u; ++i)
            copy_word(f, 0x61b940u + i * 4u, 0x5fa69cu + i * 4u);
        push(11, 31);
        put(f, 0x5e3794, 6);
        return;
    }
    case 6: {
        const auto stage = read(f, 0x5e39ecu + read(f, 0x5e39ec) * 4u);
        const auto track = stage_track_437920(f, stage);
        if (read(f, 0x61b944) == 1)
        {
            unlock_track(f, track);
            constexpr std::array<W, 6> cars{2, 6, 3, 14, 16, 5}; // actual PE44BCE0 target order
            if (stage - 1u < cars.size())
                unlock_car(f, cars[stage - 1u]);
        }
        const auto rank = 7u - read(f, 0x61b944); // full43DC20, EDX6
        const auto placing = read(f, 0x61b944);
        put(f, 0x5e37b0, rank);
        const auto needed = read(f, 0x5f9c14u + stage * 0x84u);
        if (as_signed(placing) > as_signed(needed))
        {
            put(f, 0x5e3794, 8);
            return;
        }
        if (needed == 1)
        {
            put(f, 0x5e37a8, 1);
            put(f, 0x5e3794, 8);
            class_unlock(f, stage);
            return;
        }
        put(f, 0x5e3794, 7);
        push(12, 40);
        return;
    }
    case 7: {
        const auto stage = read(f, 0x5e39ecu + read(f, 0x5e39ec) * 4u);
        if (as_signed(stage) > 0 && as_signed(stage) <= 6)
            return; // literal source return, no invented phase advance
        put(f, 0x5e3794, 4);
        push(13, 12);
        return;
    }
    case 8: {
        bool all = true;
        for (W stage = 1; stage < 19u; ++stage)
        {
            const auto mode = read(f, 0x5e3790), track = stage_track_437920(f, stage);
            if (source_track_available_4371c0(f, track, mode) == 0)
                all = false;
        }
        for (W at = 0x5df65b; at < 0x5df673; at += 2u)
            if (read(f, at - 1u, 1) == 0 || read(f, at, 1) == 0)
                all = false;
        if (all)
        {
            unlock_car(f, 12);
            unlock_car(f, 13);
            unlock_car(f, 17);
        }
        const auto selected = read(f, 0x5e3798), score = read(f, 0x5e37ac);
        insert_score_437790(f, 0x5e3a34u + selected * 64u, score);
        // 43BFD0 is exactly RET, not an omitted persistence operation.
        const auto completed = read(f, 0x5e37a8);
        put(f, 0x5e3b34, 0x5f);
        put(f, 0x5e3b38, completed != 0 ? 0x3bu : 0x39u);
        put(f, 0x5e3b3c, completed != 0 ? 0x3cu : 0x3au);
        put(f, 0x5e3b40, 0x3c75c28f);
        put(f, 0x5e3b44, completed != 0 ? 0x42a00000u : 0u);
        put(f, 0x5e3794, 9);
        push(completed != 0 ? 14u : 15u, 37);
        return;
    }
    case 9:
        put(f, 0x5e3bf4, 0x1fb);
        put(f, 0x5e3bf8, 0);
        put(f, 0x5e3bfc, 2);
        put(f, 0x5e3c00, 100);
        put(f, 0x5e3c04, 0x5df8d4);
        put(f, 0x5e3794, 10);
        push(16, 39);
        return;
    case 10:
        clear_persistence_record_changes(f.source.persistence_view());
        put(f, 0x5e3794, 11);
        push(12, 40);
        return;
    case 11:
        put(f, 0x5e3794, 12);
        request_frontend_save_page_43d500(f, 0, factory, pages, music);
        return;
    case 12:
        (void)pop_frontend_page_43eef0(f, pages, music);
        return;
    default:
        return; // 44B5F1 unsigned JA, including negative DWORD patterns
    }
}
} // namespace sgn::retail
