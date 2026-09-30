#include "sgn/source_menu_navigation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
std::uint32_t clamp_phase(std::uint32_t value) noexcept
{
    return static_cast<std::uint32_t>(std::clamp(as_signed(value), 0, 65536)); // 43D790
}
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
} // namespace
void update_frontend_words_436ed0(FrontendState &frontend)
{
    frontend.shared_platform()->require_owner_thread();
    auto &words = frontend.source.menu_words_5da2e8;
    // 436ED5..436EE8: four pairs, each source load followed by its store.
    for (unsigned at = 8; at < 32; at += 6)
    {
        words[at] = words[at - 2];
        words[at + 1] = words[at - 1];
    }
    auto *bytes = reinterpret_cast<unsigned char *>(words.data());
    auto &world = frontend.world();
    for (unsigned slot = 0; slot < 4; ++slot)
    { // 436EF0 ->436F00, ECX=slot+1
        const auto destination = (slot + 1) * 24 + 1;
        // 437090 uses physical+0 and +10 (activity_kind), NOT device kind+C.
        const auto active = world.menu_input.raw_words[slot].word_0;
        bool admitted = false;
        if (active != 0)
        {
            const auto kind = world.source_input.physical[slot].activity_kind;
            admitted = kind - 1u <= 4u;
        }
        if (!admitted)
        {
            for (unsigned i = 0; i < 7; ++i)
                bytes[destination + i] = 0;
            continue;
        }
        auto bank = borrow_menu_input_bank(world.menu_input.edges[slot],
                                           world.source_input.acquisition.physical[slot].extra_edges);
        // 436F60 jump table: current offsets 2,3,0,1,6,4,5 for keys 1..7.
        constexpr unsigned order[]{2, 3, 0, 1, 6, 4, 5};
        for (unsigned i = 0; i < 7; ++i)
            bytes[destination + i] = bank.current[order[i]].get();
    }
}
float approach_frontend_value_43d810(float current, float target, float minimum_step) noexcept
{
    const float difference = vehicle_f32(double(target) - double(current)); // 43D81B
    // 43D9E0 uses C0: unordered also negates. C3 at 43D831 then returns current.
    const float magnitude = (!(difference >= 0.0f)) ? -difference : difference;
    if (magnitude == 0.0f || std::isnan(magnitude))
        return current;
    volatile double product = double(magnitude) * double(as_float(0x3dcccccdu));
    double step = product + double(minimum_step);
    if (!(double(magnitude) > step))
        return target; // 43D849 includes unordered/equal
    if (!(target >= current))
        step = -step;
    return vehicle_f32(step + double(current)); // caller FSTP32
}
void update_frontend_navigation_43eff0(FrontendState &frontend, SourceNavigationPageServices &calls)
{
    frontend.shared_platform()->require_owner_thread();
    auto &source = frontend.source;
    auto word = [&](std::uint32_t address) -> std::uint32_t & { return source.source_word(address); };
    auto percent = as_signed(word(0x5ff3ec) * 100u) / 65536; // 43EFF0..43F012
    if (word(0x5ff4b4) != 0)
    {
        // 43DD70 reads 464 then clears 460; its returned predicate is discarded.
        [[maybe_unused]] const auto reset_predicate = word(0x5ff464) != 0;
        word(0x5ff460) = 0;
        calls.feedback_440880(frontend, 2);             // 43F01E..23
        initialize_frontend_lookup_tables(source);      // 4411C0
        word(0x5ff4ac) = 0;                             // full43DE40
        (void)calls.select_page_43ef10(frontend, 1, 1); // ECX=1,EDX=1
        word(0x5ff4b4) = 0;
        return;
    }
    volatile double x_step = double(as_float(word(0x5ff44c))) * double(as_float(0x3c888889));
    word(0x5ff4a4) = 0;
    word(0x5ff444) = 1;
    word(0x5ff454) = as_word(vehicle_f32(x_step + double(as_float(word(0x5ff454)))));
    volatile double y_step = double(as_float(word(0x5ff450))) * double(as_float(0x3c888889));
    word(0x5ff458) = as_word(vehicle_f32(y_step + double(as_float(word(0x5ff458)))));
    calls.update_cursor_43f610(frontend);
    const auto minimum = as_signed(word(0x5ff464));
    word(0x5ff444) = 0;
    if (percent < minimum)
        percent = minimum;
    auto &world = frontend.world();
    if (world.controls.current >= 4)
        throw FormatError("frontend overlay controller outside source owner");
    set_overlay_transition(world.overlays[world.controls.current], 2, 1, 0, 0, std::bit_cast<std::uint32_t>(percent),
                           std::bit_cast<std::uint32_t>(percent), 1.0f); // 442AD0
    auto current = word(0x5ff3f8);
    word(0x5ff444) = 1;
    if (current == 0)
        current = word(0x5ff3f0);
    if (current != 0)
    {
        calls.dispatch_navigation_43f310(frontend);
        current = word(0x5ff3f0);
    }
    const auto phase = word(0x5ff3e8);
    word(0x5ff444) = 0;
    if (phase == 2)
    {
        const auto progress = word(0x5ff3ec);
        if (as_signed(progress) < 65536)
            word(0x5ff3ec) = clamp_phase(progress + 0x2492u);
        else
        {
            if (current != 0 && word(0x5ff3f4) == 0)
            {
                calls.close_page(frontend, current);
                // The original rereads the object after its close callback.
                release_frontend_page_object_43d4c0(std::as_writable_bytes(std::span{source.page_pool}),
                                                    word(0x5ff3f0));
            }
            const auto next = word(0x5ff3f8), next_owned_elsewhere = word(0x5ff3fc);
            word(0x5ff3f0) = next;
            word(0x5ff3f4) = next_owned_elsewhere;
            word(0x5ff3f8) = 0;
            word(0x5ff3fc) = 0;
            clear_frontend_navigation_tail_43dee0(source);
            // Unconditional source virtual call: a null/unknown identity must fail.
            calls.page_event(frontend, word(0x5ff3f0), 19, 0);
            word(0x5ff3e8) = 3;
            word(0x5ff468) = 0;
        }
    }
    else if (phase == 3)
    {
        const auto progress = word(0x5ff3ec);
        if (as_signed(progress) <= 0)
            word(0x5ff3e8) = 1;
        else
            word(0x5ff3ec) = clamp_phase(progress - 0x2492u);
    }
    const auto fade = approach_frontend_value_43d810(as_float(word(0x5ff468)), 1.0f, as_float(0x3ba3d70a));
    const auto suppress_events = word(0x5ff4a4); // 43F1C8 precedes the fade store
    word(0x5ff468) = as_word(fade);
    if (suppress_events == 0)
    {
        current = word(0x5ff3f0);
        if (current != 0)
        {
            calls.page_event(frontend, current, 2, 0);
            current = word(0x5ff3f0);
        }
        auto next = word(0x5ff3f8);
        if (next != 0)
        {
            calls.page_event(frontend, next, 2, 0);
            current = word(0x5ff3f0);
            next = word(0x5ff3f8);
        }
        word(0x5ff444) = 1;
        if (current != 0)
            calls.page_event(frontend, next != 0 ? next : current, 1, 0);
        word(0x5ff444) = 0;
    }
    for (std::uint32_t offset = 0; offset < 24; offset += 4)
    {
        const auto value = as_float(word(0x5ff470 + offset)), target = as_float(word(0x5ff488 + offset));
        word(0x5ff470 + offset) = as_word(approach_frontend_value_43d810(value, target, as_float(0x3a83126f)));
    }
    const auto ticks = word(0x5ff45c), reset_ticks = word(0x5ff460);
    word(0x5ff45c) = ticks + 1u;
    word(0x5ff460) = reset_ticks + 1u;
}
void SourceMenuNavigationChildren::update_words_436ed0(FrontendState &frontend)
{
    update_frontend_words_436ed0(frontend);
}
void SourceMenuNavigationChildren::update_navigation_43eff0(FrontendState &frontend)
{
    update_frontend_navigation_43eff0(frontend, *this);
}
} // namespace sgn::retail
