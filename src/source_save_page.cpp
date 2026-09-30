#include "sgn/source_save_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_word;
W intfloat(W v)
{
    return as_word(static_cast<float>(as_signed(v)));
}
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
using sgn::retail::frontend_page_words::source_word;
bool automatic_457a20(W mode)
{
    return as_signed(mode) > 0 && as_signed(mode) <= 3;
}
void clear_operation_458860(FrontendState &f, W p)
{
    set_page_word(f, p, 0x1ec, 0);
    set_page_word(f, p, 0x1e8, 0);
    set_page_word(f, p, 0x1f0, 0);
}
W card_state_457bf0(FrontendState &f, W card, W &free, SourceSavePageServices &io)
{
    // 440300 returns signed(card)<=8. Scan457A40 visits exactly0..7.
    if (as_signed(card) > 8)
        return 0;
    io.select_card_4402f0(card);
    free = 0;
    const auto status = io.card_capacity_440200(free);
    if (status == 1)
        return 2;
    if (status != 0)
        return 1;
    const auto mode = source_word(f, 0x5e3c28);
    if (mode == 1 || mode == 2 || mode == 3)
    {
        const auto found = io.setup_status_440320();
        if (mode == 2)
            return found == 4 ? 5u : 4u;
        if (found == 4)
            return 5;
        return as_signed(free) >= as_signed(0x211c) ? 4u : 3u; // 440310 identity
    }
    if (mode == 4 || mode == 5)
    {
        const auto found = io.replay_status_4405c0(source_word(f, 0x5e3c2c));
        if (mode == 4)
            return found == 4 ? 5u : 4u;
        if (found == 4)
            return 5;
        const auto required = io.replay_blocks_4405b0(source_word(f, 0x5e3c34));
        return as_signed(free) >= as_signed(required) ? 4u : 3u;
    }
    return 1;
}
W scan_cards_457a40(FrontendState &f, W p, SourceSavePageServices &io)
{
    W changed = 0;
    set_page_word(f, p, 0x1fc, 0);
    for (W at = 0x64; at < 0x1e4; at += 4)
        set_page_word(f, p, at, 0);
    for (W i = 0; i < 8; ++i)
        set_page_word(f, p, 0x6cu + i * 0x30u, 0);
    set_page_word(f, p, 0x1e4, 0);
    for (W card = 0; card < 8; ++card)
    {
        W free = 0;
        const auto state = card_state_457bf0(f, card, free, io);
        if (state != page_word(f, p, 0x44u + card * 4u))
            changed = 1;
        if (state == 0)
            continue; // source retains previous44 slot on absence
        set_page_word(f, p, 0x44u + card * 4u, state);
        set_page_word(f, p, 0x64u + page_word(f, p, 0x1e4) * 0x30u, card);
        set_page_word(f, p, 0x6cu + page_word(f, p, 0x1e4) * 0x30u, state);
        set_page_word(f, p, 0x68u + page_word(f, p, 0x1e4) * 0x30u, free);
        const auto name = frontend_language_text_441170(f.source.language_strings(), card + 0x255u);
        set_page_word(f, p, 0x90u + page_word(f, p, 0x1e4) * 0x30u, name.value);
        const auto record = page_word(f, p, 0x1e4) * 0x30u;
        if (page_word(f, p, record + 0x6c) == 5)
        {
            const auto mode = source_word(f, 0x5e3c28);
            if (mode >= 1 && mode <= 3)
                set_page_word(f, p, 0x1fc, page_word(f, p, 0x1fc) + 1u);
            else if (mode == 4 || mode == 5)
                io.replay_row_457b36(f, p, record);
        }
        set_page_word(f, p, 0x1e4, page_word(f, p, 0x1e4) + 1u);
    }
    const auto n = page_word(f, p, 0x1e4), selection = page_word(f, p, 0x20);
    if (as_signed(selection) >= as_signed(n))
        set_page_word(f, p, 0x20, n - 1u);
    if (as_signed(page_word(f, p, 0x20)) < 0)
        set_page_word(f, p, 0x20, 0);
    return changed;
}
W find_card_457e70(FrontendState &f, W p, W card)
{
    const auto n = page_word(f, p, 0x1e4);
    for (W i = 0; as_signed(i) < as_signed(n); ++i)
        if (page_word(f, p, 0x64u + i * 0x30u) == card)
            return i;
    return 0xffffffffu;
}
W initial_card_457cf0(FrontendState &f, W p, W preference)
{
    const auto mode = source_word(f, 0x5e3c28);
    W selected = 0xffffffffu;
    const auto n = page_word(f, p, 0x1e4);
    if (mode >= 1 && mode <= 5)
    {
        selected = find_card_457e70(f, p, source_word(f, mode <= 3 ? 0x5e3c38 : 0x5e3c3c));
        if (as_signed(selected) >= 0)
            return selected;
    }
    if ((source_word(f, 0x5e3c28) == 3 || source_word(f, 0x5e3c28) == 5) && source_word(f, 0x5e3c1c) != 0)
        source_word(f, 0x5e3c1c) = 0;
    auto find = [&](W state) {
        for (W i = 0; as_signed(i) < as_signed(n) && as_signed(selected) < 0; ++i)
            if (page_word(f, p, 0x6cu + i * 0x30u) == state)
                selected = i;
    };
    if (preference == 1)
    {
        find(5);
        find(4);
        find(2);
    }
    else if (preference == 2)
        find(5);
    else if (preference == 3)
    {
        find(4);
        find(2);
        find(5);
    }
    return as_signed(selected) < 0 ? 0u : selected;
}
struct Flow
{
    FrontendState &f;
    W p;
    SourcePageFactoryServices &factory;
    SourceNavigationPageServices &pages;
    CampaignScoreMusic &music;
    SourceMainPageServices &ui;
    SourceSavePageServices &io;
    W read(W a)
    {
        return page_word(f, p, a);
    }
    void write(W a, W v)
    {
        set_page_word(f, p, a, v);
    }
    void child(W event, W arg = 0)
    {
        (void)ui.menu_text_event(f, read(0x40), event, arg);
    }
    void leave()
    {
        const auto kind = source_word(f, 0x5e3c24);
        if (kind != 0)
            (void)select_frontend_page_43ef10(f, 0, kind, factory, pages, music);
        else
            (void)pop_frontend_page_43eef0(f, pages, music);
    }
    bool rescan()
    {
        if (io.changed_cards_440170() == 0)
            return false;
        if (scan_cards_457a40(f, p, io) == 0)
            return false;
        if (read(0x1ec) != 0)
            clear_operation_458860(f, p);
        pages.feedback_440880(f, 6);
        return true;
    }
    void operation(W card, W kind, W confirmation)
    {
        write(0x1f4, card);
        write(0x1e8, confirmation);
        write(0x1ec, kind);
        write(0x1f0, 0);
        write(0x1f8, 10);
        pages.feedback_440880(f, 3);
        child(28, kind == 1 ? 1u : 0u);
    }
    void selected_action(W card)
    {
        const auto state = read(0x6cu + read(0x20) * 0x30u), mode = source_word(f, 0x5e3c28);
        if (state - 1u > 4u)
            return;
        if (mode == 1 || mode == 3 || mode == 5)
        {
            if (state == 1 || state == 3)
            {
                pages.feedback_440880(f, 9);
                return;
            }
            if (state == 2)
            {
                operation(card, 1, 1);
                return;
            }
            if (mode == 1)
            {
                operation(card, state == 4 ? 2u : 3u, 0);
                return;
            }
            operation(card, mode == 3 ? 2u : 4u, state == 5 ? 1u : 0u);
            return;
        }
        if (mode == 2 || mode == 4)
        {
            if (state <= 4)
                pages.feedback_440880(f, 9);
            else
                operation(card, mode == 2 ? 3u : 5u, 0);
        }
    }
    void idle(W event)
    {
        if (event == 1)
        {
            if (source_word(f, 0x5e3c1c) == 0 || read(0x1ec) != 0 || read(0x1e4) == 0)
                return;
            decrement_frontend_feedback_gate_440910(f.source);
            const auto card = read(0x64u + read(0x20) * 0x30u), mode = source_word(f, 0x5e3c28);
            if (mode == 1)
                operation(card, 3, 0);
            else if (mode == 3)
                operation(card, 2, 0);
            else if (mode == 5)
                operation(card, 4, 0);
            increment_frontend_feedback_gate_440900(f.source);
            return;
        }
        if (event == 17)
        {
            operation(0, 6, source_word(f, 0x5e3c20));
            return;
        }
        if (event == 16)
        {
            if (read(0x1e4) != 0)
                selected_action(read(0x64u + read(0x20) * 0x30u));
            return;
        }
        if (event == 11)
        {
            const auto n = read(0x1e4), s = read(0x20);
            if (n != 0 && as_signed(s) > 0)
            {
                write(0x1c, 0);
                write(0x20, s - 1u);
                ui.feedback_440890(f, event);
            }
            return;
        }
        if (event == 12)
        {
            const auto n = read(0x1e4), s = read(0x20);
            if (n != 0 && as_signed(s) < as_signed(n - 1u))
            {
                write(0x1c, 0);
                write(0x20, s + 1u);
                ui.feedback_440890(f, event);
            }
        }
    }
    void storage()
    {
        const auto kind = read(0x1ec), card = read(0x1f4);
        if (kind >= 1 && kind <= 5)
        {
            io.select_card_4402f0(card);
            W status = 0;
            switch (kind)
            {
            case 1:
                status = io.format_card_440180();
                break;
            case 2:
                status = static_cast<W>(save_persistence(f.source.persistence_view(), io.persistence(), card));
                break;
            case 3:
                status = static_cast<W>(load_persistence(f.source.persistence_view(), io.persistence(), card));
                break;
            case 4:
                status = io.replay_save_458e10(f);
                break;
            case 5:
                status = io.replay_load_458e30(f);
                break;
            default:
                break;
            }
            write(0x1f0, status);
        }
        else if (kind == 6)
            write(0x1f0, 0);
        if (read(0x1ec) != 6)
            io.sleep_source_458de9(500);
    }
    void execute(W event)
    {
        if (event != 1)
            return;
        const auto delay = read(0x1f8);
        if (delay != 0)
        {
            write(0x1f8, delay - 1u);
            return;
        }
        storage();
        if (read(0x1f0) != 0)
        {
            pages.feedback_440880(f, 7);
            child(28, 0);
            return;
        }
        const auto kind = read(0x1ec);
        if (kind != 6)
            pages.feedback_440880(f, 8);
        if (kind == 1)
        {
            (void)scan_cards_457a40(f, p, io);
            clear_operation_458860(f, p);
            return;
        }
        if (kind != 6)
            source_word(f, 0x5e3c40) = 1;
        const auto mode = source_word(f, 0x5e3c28);
        if (mode >= 1 && mode <= 5)
            source_word(f, mode <= 3 ? 0x5e3c38 : 0x5e3c3c) = read(0x64u + read(0x20) * 0x30u);
        leave();
        clear_operation_458860(f, p);
    }
    void dialog(W event, bool error)
    {
        if (event == 11 || event == 12)
        {
            child(event);
            return;
        }
        if (event == 17)
        {
            ui.feedback_440890(f, event);
            clear_operation_458860(f, p);
            return;
        }
        if (event == 16)
        {
            const auto selected = ui.menu_text_event(f, read(0x40), 26, 0);
            if (selected == 1)
            {
                pages.feedback_440880(f, 2);
                clear_operation_458860(f, p);
            }
            else if (selected == 0)
            {
                if (!error)
                    pages.feedback_440880(f, 3);
                write(error ? 0x1f0 : 0x1e8, 0);
            }
            return;
        }
        if (event == 3)
        {
            const auto kind = read(0x1ec);
            if (error)
            {
                io.draw_confirmation_459070(f, p, kind + 0x23bu);
                return;
            }
            if (kind == 0 || read(0x1e8) == 0)
                return;
            if (kind == 6)
            {
                const auto mode = source_word(f, 0x5e3c28);
                if (mode >= 1 && mode <= 5)
                    io.draw_confirmation_459070(f, p, 0x236u + mode);
            }
            else
                io.draw_confirmation_459070(f, p, kind + 0x230u);
        }
    }
    void nested(W event)
    {
        if (read(0x14) != 0)
            return;
        if (read(0x1f0) != 0)
        {
            dialog(event, true);
            return;
        }
        if (read(0x1ec) != 0)
        {
            if (read(0x1e8) != 0)
                dialog(event, false);
            else
                execute(event);
            return;
        }
        idle(event);
    }
};
} // namespace
W create_frontend_save_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                   SourceMainPageServices &ui, SourceSavePageServices &io)
{
    f.shared_platform()->require_owner_thread();
    if (!(io.application_literals() == f.shared_platform()->source_application_literals()))
        throw FormatError("PageSave services do not borrow the active application owner");
    const auto p = allocate_frontend_page_object_43d470(page_pool(f), 0x200);
    auto b = frontend_page_object_bytes(page_pool(f), p, 0x200);
    std::fill(b.begin(), b.end(), std::byte{0});
    const auto event = source_word(f, 0x488c94);
    set_page_word(f, p, 8, 0);
    const auto close = source_word(f, 0x488c90);
    set_page_word(f, p, 12, close);
    set_page_word(f, p, 16, event);
    set_page_word(f, p, 4, 26);
    set_page_word(f, p, 0, 0x4797fc);
    set_page_word(f, p, 0x18, 0x200); // full4402E0 returns512
    source_word(f, 0x5e3c40) = 0;
    for (W at = 0x1c; at <= 0x34; at += 4)
        set_page_word(f, p, at, 0);
    set_page_word(f, p, 0x38, 0x3f800000);
    set_page_word(f, p, 0x3c, 0x3f800000);
    for (W at = 0x44; at < 0x64; at += 4)
        set_page_word(f, p, at, 1);
    (void)scan_cards_457a40(f, p, io);
    const auto selected = initial_card_457cf0(f, p, source_word(f, 0x5e3c28));
    set_page_word(f, p, 0x20, selected);
    set_page_word(f, p, 0x28, intfloat(selected));
    set_page_word(f, p, 0x2c, intfloat(selected));
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8);
    b = frontend_page_object_bytes(page_pool(f), child, 0xc8);
    std::fill(b.begin(), b.end(), std::byte{0});
    const auto childclose = source_word(f, 0x4794e8), childevent = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, childclose);
    set_page_word(f, child, 16, childevent);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x488c98);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x488c60, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, p, 0x40, 0);
    }
    else
        set_page_word(f, p, 0x40, child);
    const auto published = page_word(f, p, 0x40);
    (void)ui.menu_text_event(f, published, 24, 0);
    (void)ui.menu_text_event(f, published, 19, 0);
    if (automatic_457a20(source_word(f, 0x5e3c28)))
        source_word(f, 0x5e3c1c) = 1;
    set_page_word(f, p, 8, key);
    return p;
}
void destroy_frontend_save_page_457ea0(FrontendState &f, W p)
{
    const auto child = page_word(f, p, 0x40);
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("PageSave reached unknown MenuText destructor");
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, p, 0x40, 0);
}
W dispatch_frontend_save_page_457ec0(FrontendState &f, W p, W event, SourcePageFactoryServices &factory,
                                     SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                     SourceMainPageServices &ui, SourceSavePageServices &io)
{
    f.shared_platform()->require_owner_thread();
    if (!(io.application_literals() == f.shared_platform()->source_application_literals()))
        throw FormatError("PageSave services do not borrow the active application owner");
    Flow flow{f, p, factory, pages, music, ui, io};
    if (event == 1)
    {
        if (flow.rescan())
            return 0;
        flow.child(event);
        flow.nested(event);
        if (source_word(f, 0x5e3c28) == 1 && source_word(f, 0x5e3c1c) != 0 && flow.read(0x1ec) != 0 &&
            (flow.read(0x1f0) != 0 || flow.read(0x1e8) != 0))
            flow.write(0x14, 1);
        if (flow.read(0x14) != 0)
            flow.leave();
    }
    else if (event == 2)
    {
        flow.child(event);
        const auto target = intfloat(flow.read(0x20)), age = flow.read(0x1c), current = flow.read(0x28);
        flow.write(0x1c, age + 1u);
        flow.write(0x2c, target);
        flow.write(0x28,
                   as_word(approach_frontend_value_43d810(as_float(current), as_float(target), as_float(0x3e4ccccd))));
        const auto target2 = flow.read(0x34), current2 = flow.read(0x30);
        flow.write(
            0x30, as_word(approach_frontend_value_43d810(as_float(current2), as_float(target2), as_float(0x3a83126f))));
        const auto target3 = as_float(flow.read(0x3c)), current3 = as_float(flow.read(0x38));
        // 43D8F0 PC53 comparison/arithmetic with explicit binary32 store.
        double value = current3;
        if (current3 != target3 && !std::isnan(current3) && !std::isnan(target3))
        {
            const double difference = double(target3) - double(current3), step = as_float(0x3ccccccd);
            value =
                step < std::abs(difference) ? double(current3) + (target3 < current3 ? -step : step) : double(target3);
        }
        const auto stored = as_word(static_cast<float>(value));
        flow.write(0x38, stored);
        if (automatic_457a20(source_word(f, 0x5e3c28)) && source_word(f, 0x5e3c1c) != 0)
        {
            source_word(f, 0x5ff448) = 0;
            source_word(f, 0x5ff44c) = 0;
            source_word(f, 0x5ff450) = 0;
            return 0;
        }
        if (flow.read(0x24) != 0)
        {
            source_word(f, 0x5ff448) = flow.read(0x1ec) != 0 ? 105u : 92u;
            source_word(f, 0x5ff44c) =
                as_word(static_cast<float>(double(as_float(stored)) * double(as_float(0x3f400000))));
            source_word(f, 0x5ff450) =
                as_word(static_cast<float>(double(as_float(stored)) * double(as_float(0x3ee66667))));
        }
    }
    else if (event == 11 || event == 12 || event == 16 || event == 17)
    {
        if (!flow.rescan())
            flow.nested(event);
    }
    else if (event == 19)
    {
        source_word(f, 0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x112a43);
        flow.write(0x24, 1);
        flow.write(0x34, 0x3f800000);
    }
    else if (event == 20)
        flow.write(0x34, 0);
    else if (event == 3)
    {
        if (automatic_457a20(source_word(f, 0x5e3c28)) && source_word(f, 0x5e3c1c) != 0)
            return 0;
        if (flow.read(0x1ec) != 0 && flow.read(0x1e8) == 0 && flow.read(0x1f0) == 0)
            io.draw_progress_458730(f, p);
        io.draw_cards_4581b0(f, p);
        const auto mode = source_word(f, 0x5e3c28);
        if (mode >= 1 && mode <= 3)
            ui.background_4392b0(f, 0x205u + mode * 7u, 0);
        else if (mode == 4 || mode == 5)
            io.replay_background_4580fc(f, mode);
        flow.nested(event);
    }
    return 0;
}
} // namespace sgn::retail
