#include "sgn/source_main_page.hpp"
#include "sgn/source_single_page.hpp"
#include "sgn/source_selection_pages.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::retail::frontend_page_words::page_object_bytes;
using sgn::retail::frontend_page_words::page_pool;
std::uint32_t get(std::span<const std::byte> bytes, std::size_t at)
{
    if (at > bytes.size() || bytes.size() - at < 4)
        throw FormatError("PageMain object read exceeds source owner");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= std::to_integer<std::uint32_t>(bytes[at + i]) << (8 * i);
    return value;
}
void put(std::span<std::byte> bytes, std::size_t at, std::uint32_t value)
{
    if (at > bytes.size() || bytes.size() - at < 4)
        throw FormatError("PageMain object write exceeds source owner");
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = static_cast<std::byte>((value >> (8 * i)) & 255u);
}
std::uint32_t word(FrontendState &f, std::uint32_t address)
{
    return f.source.source_word(address);
}
std::uint32_t first_menu_text_row_43c900(FrontendState &f, std::uint32_t descriptor)
{
    auto row = word(f, descriptor + 0x28u);
    std::uint32_t index = 0;
    auto text = word(f, row + 12u); // 43C906
    if (text == 0)
        return 0xffffffffu;
    for (;;)
    {
        if (std::bit_cast<std::int32_t>(word(f, row)) >= 0)
            return index;
        text = word(f, row + 0x2cu); // 43C914 before moving to next row
        row += 0x20u;
        ++index;
        if (text == 0)
            return 0xffffffffu;
    }
}
void initialization_event(FrontendState &f, std::uint32_t child, std::uint32_t event)
{
    const auto bytes = page_object_bytes(f, child, 0xc8);
    if (get(bytes, 16) != 0x43c980u)
        throw FormatError("PageMain reached unimplemented MenuText initialization callback");
    (void)get(bytes, 0x14);
    (void)get(bytes, 0x18); // 43C991/994 before event dispatch
    // Source jump table43CED0/43CE8C: event22->43C9B6; event24->43CC3B.
    // Both return EAX0. Only these two events are invoked by4530B0.
    if (event == 22)
        put(bytes, 0xb0, 1);
    else if (event == 24)
        put(bytes, 0xb4, 1);
    else
        throw FormatError("PageMain constructor requested unsupported initialization event");
}
std::uint32_t create_menu_text(FrontendState &f, std::uint32_t descriptor, std::uint32_t name, bool first,
                               SourceNavigationPageServices &calls)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8);
    std::uint32_t close = 0;
    if (!first)
        close = word(f, 0x4794e8); // 453134 before REP clear
    const auto bytes = page_object_bytes(f, child, 0xc8);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    if (first)
    {
        close = word(f, 0x4794e8);
        const auto event = word(f, 0x4794ec);
        put(bytes, 12, close);
        put(bytes, 16, event);
        put(bytes, 4, 41);
        put(bytes, 0, name);
        put(bytes, 8, 0);
    }
    else
    {
        const auto event = word(f, 0x4794ec);
        put(bytes, 12, close);
        put(bytes, 4, 41);
        put(bytes, 0, name);
        put(bytes, 8, 0);
        put(bytes, 16, event);
    }
    // 43C860 returns1 after the actual row selection and optional sound tail.
    if (initialize_frontend_menu_text_43c860(f, child, descriptor, calls) != 0)
        return child;
    release_frontend_page_object_43d4c0(page_pool(f), child);
    return 0;
}
void close_menu_text(FrontendState &f, std::uint32_t child)
{
    if (get(page_object_bytes(f, child, 16), 12) != 0x43c970u)
        throw FormatError("PageMain reached unimplemented MenuText destructor callback");
    // Complete43C970 is RET. Caller4531D0, not the callback, frees the object.
}
} // namespace
void select_frontend_menu_text_43c930(FrontendState &f, std::uint32_t id, std::uint32_t selection,
                                      SourceNavigationPageServices &calls)
{
    const auto bytes = page_object_bytes(f, id, 0xc8);
    const auto descriptor = get(bytes, 0x18);
    put(bytes, 0xc0, 0x3f800000);
    put(bytes, 0xc4, 0);
    put(bytes, 0xa4, selection);
    const auto rows = word(f, descriptor + 0x28u);
    const auto signal = word(f, rows + (selection << 5) + 0x18u); // wrapping x86 address, including selection=-1
    if (signal != 0)
        calls.feedback_440880(f, 17); // actual43C960 tail, never a no-op
}
std::uint32_t initialize_frontend_menu_text_43c860(FrontendState &f, std::uint32_t id, std::uint32_t descriptor,
                                                   SourceNavigationPageServices &calls)
{
    const auto bytes = page_object_bytes(f, id, 0xc8);
    const auto x = word(f, descriptor + 12u);
    put(bytes, 0xb8, x);
    const auto y = word(f, descriptor + 16u);
    put(bytes, 0x18, descriptor);
    auto rows = word(f, descriptor + 0x28u);
    put(bytes, 0xbc, y);
    put(bytes, 0x1c, 0x3f800000);
    put(bytes, 0x20, 0x3f800000);
    put(bytes, 0xa4, 0);
    put(bytes, 0xa8, 0xffffffffu);
    put(bytes, 0xac, 0);
    put(bytes, 0xb0, 0);
    put(bytes, 0xb4, 0);
    rows += 12u;
    std::uint32_t count = 0;
    while (word(f, rows) != 0)
    {
        ++count;
        rows += 0x20u;
        if (count == 32)
            break;
    }
    put(bytes, 0x14, count);
    for (std::uint32_t i = 0; i < get(bytes, 0x14); ++i)
        put(bytes, 0x24u + i * 4u, 0x3f800000);
    const auto selected = first_menu_text_row_43c900(f, get(bytes, 0x18));
    select_frontend_menu_text_43c930(f, id, selected, calls);
    return 1;
}
std::uint32_t count_frontend_players_437100(FrontendState &f)
{
    f.shared_platform()->require_owner_thread();
    // 456000 tail437100 calls437090 with source input IDs1..4 (native slots0..3).
    auto &world = f.world();
    std::uint32_t count = 0;
    for (unsigned slot = 0; slot < 4; ++slot)
    {
        if (world.menu_input.raw_words[slot].word_0 == 0)
            continue;
        const auto kind = world.source_input.physical[slot].activity_kind;
        if (kind - 1u <= 4u)
            ++count;
    }
    return count;
}
void refresh_frontend_main_page_4531a0(FrontendState &f)
{
    const auto count = count_frontend_players_437100(f);
    const auto rows = word(f, 0x487c10);
    f.source.source_word(rows + 0x20u) = count < 2 ? 0xffffffffu : 1u;
}
std::uint32_t create_frontend_main_page_43e000(FrontendState &f, std::uint32_t key, SourceNavigationPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x20);
    const auto bytes = page_object_bytes(f, page, 0x20);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto event = word(f, 0x487c1c);
    put(bytes, 8, 0);
    const auto close = word(f, 0x487c18);
    put(bytes, 12, close);
    put(bytes, 16, event);
    put(bytes, 4, 2);
    put(bytes, 0, 0x479694);
    // Full4530B0: child publication occurs AFTER its initializer returns.
    auto child = create_menu_text(f, 0x487be8, 0x487c20, true, calls);
    put(bytes, 0x14, child);
    child = get(bytes, 0x14);
    initialization_event(f, child, 24);
    initialization_event(f, child, 22);
    put(bytes, 0x1c, 0);
    child = create_menu_text(f, 0x487ad8, 0x487c2c, false, calls);
    put(bytes, 0x18, child);
    child = get(bytes, 0x18);
    initialization_event(f, child, 24);
    refresh_frontend_main_page_4531a0(f);
    put(bytes, 8, key); // 43ED94 AFTER successful constructor
    return page;
}
void destroy_frontend_main_page_children_4531d0(FrontendState &f, std::uint32_t page)
{
    const auto bytes = page_object_bytes(f, page, 0x20);
    auto child = get(bytes, 0x14);
    close_menu_text(f, child);
    release_frontend_page_object_43d4c0(page_pool(f), child);
    child = get(bytes, 0x18);
    put(bytes, 0x14, 0); // 4531E3 reads second child BEFORE clearing first
    close_menu_text(f, child);
    release_frontend_page_object_43d4c0(page_pool(f), child);
    put(bytes, 0x18, 0);
    // Parent page release remains with43EE40/43EF10 caller.
}
std::uint32_t create_frontend_game_loop_page_43e000(FrontendState &f, std::uint32_t key)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24);
    const auto bytes = page_object_bytes(f, page, 24);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto event = word(f, 0x487a54);
    put(bytes, 8, 0);
    const auto close = word(f, 0x487a50);
    put(bytes, 12, close);
    put(bytes, 16, event);
    put(bytes, 4, 31);
    put(bytes, 0, 0x479824);
    put(bytes, 0x14, 0); // complete452FC0, then EAX=1 and RET
    put(bytes, 8, key);  // 43ED94, no game launch occurs in constructor
    return page;
}
std::uint32_t create_frontend_single_page_43e000(FrontendState &f, std::uint32_t key,
                                                 SourceNavigationPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24);
    const auto close = word(f, 0x488f78); // 43E17B BEFORE REP clear
    const auto bytes = page_object_bytes(f, page, 24);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    put(bytes, 8, 0);
    const auto event = word(f, 0x488f7c);
    put(bytes, 12, close);
    put(bytes, 4, 3);
    put(bytes, 0, 0x4796a0);
    put(bytes, 16, event);
    // 45A770 has the same MenuText construction order as4530B0's first child.
    auto child = create_menu_text(f, 0x488f48, 0x488f80, true, calls);
    put(bytes, 0x14, child);
    child = get(bytes, 0x14);
    initialization_event(f, child, 24);
    initialization_event(f, child, 22);
    put(bytes, 8, key); // successful initializer returns1, then43ED94
    return page;
}
void destroy_frontend_single_page_children_45a7f0(FrontendState &f, std::uint32_t page)
{
    const auto bytes = page_object_bytes(f, page, 24);
    const auto child = get(bytes, 0x14);
    close_menu_text(f, child);
    release_frontend_page_object_43d4c0(page_pool(f), child);
    put(bytes, 0x14, 0);
    // 45A7F0 does not release its own parent page.
}

namespace
{
using sgn::bit_reinterpret::as_signed;
std::uint32_t integer_float_word(std::uint32_t v)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(as_signed(v)));
}
// 4375A0 / 437710: preserve forward adjacent swaps and signed key comparison.
template <std::size_t N>
void source_selection_sort(FrontendState &f, std::uint32_t base, std::uint32_t count,
                           const std::array<std::uint32_t, N> &ranks)
{
    auto rank = [&](std::uint32_t id) { return id - 1u < N ? ranks[id - 1u] : 0u; };
    for (auto remaining = count - 1u; as_signed(remaining) >= 0; --remaining)
    {
        for (std::uint32_t i = 0; i < remaining; ++i)
        {
            const auto address = base + i * 4u;
            const auto next = word(f, address + 4u);
            const auto current = word(f, address);
            const auto next_key = rank(next);
            const auto current_key = rank(current);
            if (as_signed(current_key) > as_signed(next_key))
            {
                f.source.source_word(address) = next;
                f.source.source_word(address + 4u) = current;
            }
        }
    }
}
void selection_initialization_event_28(FrontendState &f, std::uint32_t child, std::uint32_t selected,
                                       SourceNavigationPageServices &calls)
{
    const auto bytes = page_object_bytes(f, child, 0xc8);
    if (get(bytes, 16) != 0x43c980u)
        throw FormatError("Car constructor reached unknown MenuText event callback");
    const auto count = get(bytes, 0x14);
    const auto descriptor = get(bytes, 0x18);
    // 43CA19..43CA47: rejected selection returns0 without changing selection.
    if (as_signed(selected) < 0 || as_signed(selected) >= as_signed(count))
        return;
    const auto rows = word(f, descriptor + 0x28u);
    if (as_signed(word(f, rows + (selected << 5))) < 0)
        return;
    select_frontend_menu_text_43c930(f, child, selected, calls);
}
std::uint32_t car_configuration_value_437de0(FrontendState &f, std::uint32_t sponsor, std::uint32_t car,
                                             std::uint32_t configuration)
{
    const auto items = std::as_bytes(f.source.source_bytes(0x5f3af4, 0x57e4));
    const auto rules = std::as_bytes(f.source.source_bytes(0x5f943c, 0x660));
    const auto config = std::as_bytes(f.source.source_bytes(configuration, 0xec));
    const auto model = std::as_bytes(f.source.source_bytes(0x5f3af4u + car * 300u, 300));
    std::array<std::byte, 0x218> temporary;
    compose_campaign_configuration_436490({std::span<const std::byte, 0x57e4>{items.data(), items.size()},
                                           std::span<const std::byte, 0x660>{rules.data(), rules.size()}},
                                          sponsor, std::span<const std::byte, 0xec>{config.data(), config.size()},
                                          std::span<const std::byte, 300>{model.data(), model.size()}, temporary);
    return get(temporary, 0xec); // 437E0B, actual model index is car, not car-1
}
std::array<std::uint32_t, 3> car_statistics_436d60(FrontendState &f, std::uint32_t configuration)
{
    std::array<std::byte, 0xec> local;
    for (std::uint32_t offset = 0; offset < 0xec; offset += 4)
        put(local, offset, word(f, configuration + offset));
    for (const auto offset :
         std::array<std::uint32_t, 11>{0x60, 0x50, 0xd0, 0x90, 0x40, 0x70, 0xe0, 0xa0, 0xc0, 0x80, 0xb0})
        if (get(local, offset) == 0)
            put(local, offset, offset == 0xe0 ? 3u : 5u);
    const auto s = get(local, 0xb0), d = get(local, 0xc0), q = get(local, 0x60);
    std::array<std::uint32_t, 3> values{2u * (s + 4u * d + q),
                                        2u * (get(local, 0x80) + 3u * (6u - get(local, 0xd0)) + 2u * get(local, 0x90)),
                                        6u * d + 4u * (s + q)};
    switch (word(f, configuration + 4u))
    {
    case 1:
        values[0] += 15;
        values[1] += 40;
        values[2] += 5;
        break;
    case 2:
        values[0] += 40;
        values[1] += 20;
        values[2] += 10;
        break;
    case 3:
        values[0] += 25;
        values[1] += 15;
        values[2] += 30;
        break;
    default:
        break;
    }
    return values;
}
void initialize_car_page_451700(FrontendState &f, std::uint32_t page, SourceNavigationPageServices &calls)
{
    const auto bytes = page_object_bytes(f, page, 0x1d0);
    put(bytes, 0x18, 0);
    put(bytes, 0x1c, 0);
    put(bytes, 0x34, 0);
    for (std::uint32_t i = 0; i < 3; ++i)
    {
        put(bytes, 0x38u + i * 4u, 0);
        put(bytes, 0x44u + i * 4u, 0);
    }
    put(bytes, 0x20, word(f, 0x5e3798));
    const auto count = word(f, 0x5e38f8);
    f.source.source_word(0x5e38f4) = 0;
    source_selection_sort(f, 0x5e38fc, count, std::array<std::uint32_t, 21>{12, 4, 7,  8, 15, 5,  1,  14, 13, 3, 2,
                                                                            11, 6, 10, 9, 16, 17, 18, 19, 20, 21});
    for (std::uint32_t i = 0; i < 4; ++i)
    {
        put(bytes, 0x54u + i * 0x30u, i + 1u);
        put(bytes, 0x50u + i * 0x30u, 0);
    }
    put(bytes, 0x110, 4);
    for (std::uint32_t i = 0; as_signed(i) < as_signed(word(f, 0x5e38f8)); ++i)
    {
        const auto car = word(f, 0x5e38fcu + i * 4u), category = source_car_class_437cc0(car);
        // Complete451A50. Keep x86 wrapping address, including category0.
        const auto record = 0x50u + (category - 1u) * 0x30u;
        const auto old = get(bytes, record);
        put(bytes, record + 8u + old * 4u, car);
        put(bytes, record, get(bytes, record) + 1u);
    }
    for (std::uint32_t i = 0; as_signed(i) < as_signed(get(bytes, 0x110)); ++i)
    {
        if (as_signed(get(bytes, 0x50u + i * 0x30u)) > 0)
            continue;
        for (auto j = i; as_signed(j) < as_signed(get(bytes, 0x110) - 1u); ++j)
            for (std::uint32_t k = 0; k < 12; ++k)
                put(bytes, 0x50u + j * 0x30u + k * 4u, get(bytes, 0x80u + j * 0x30u + k * 4u));
        put(bytes, 0x110, get(bytes, 0x110) - 1u);
        --i;
    }
    for (std::uint32_t group = 0; as_signed(group) < as_signed(get(bytes, 0x110)); ++group)
    {
        const auto record = 0x50u + group * 0x30u, n = get(bytes, record);
        if (as_signed(n) <= 0)
            continue;
        const auto player = word(f, 0x5e3798), remembered = word(f, 0x5e3954u + player * 4u);
        for (std::uint32_t i = 0; as_signed(i) < as_signed(n); ++i)
            if (get(bytes, record + 8u + i * 4u) == remembered)
            {
                put(bytes, 0x18, i);
                put(bytes, 0x1c, group);
                break;
            }
    }
    const auto selected = get(bytes, 0x1c), car_position = integer_float_word(get(bytes, 0x18));
    put(bytes, 0x24, car_position);
    put(bytes, 0x2c, car_position);
    const auto group_position = integer_float_word(selected);
    put(bytes, 0x28, group_position);
    put(bytes, 0x30, group_position);
    std::uint32_t group = 0;
    for (; as_signed(group) < as_signed(get(bytes, 0x110)); ++group)
    {
        const auto source = 0x4868d8u + get(bytes, 0x54u + group * 0x30u) * 32u;
        const auto destination = 0x624b18u + group * 32u;
        for (std::uint32_t k = 0; k < 8; ++k)
            f.source.source_word(destination + k * 4u) = word(f, source + k * 4u);
        f.source.source_word(destination) = group;
    }
    for (std::uint32_t k = 0; k < 8; ++k)
        f.source.source_word(0x624b18u + group * 32u + k * 4u) = word(f, 0x486978u + k * 4u);
    auto child = create_menu_text(f, 0x486998, 0x4869d8, false, calls);
    put(bytes, 0x14, child);
    child = get(bytes, 0x14);
    initialization_event(f, child, 24);
    initialization_event(f, child, 22);
    selection_initialization_event_28(f, child, selected, calls);
    std::uint32_t value = 0;
    if (word(f, 0x5e3790) == 5)
    {
        value = word(f, 0x5f92e8);
        for (std::uint32_t car = 1; car <= 21; ++car)
        {
            const auto result =
                car_configuration_value_437de0(f, word(f, 0x5f93e4), car, 0x5f2798u + (car - 1u) * 0xecu);
            put(bytes, 0x138u + (car - 1u) * 4u, result);
        }
        const auto extra = word(f, 0x5f92ec);
        if (extra != 0)
            value += car_configuration_value_437de0(f, word(f, 0x5f93e4), extra, 0x5f92f0);
    }
    const auto text = format_campaign_value_43d5f0(value, std::as_writable_bytes(std::span{f.source.numeric_scratch}));
    const auto terminator = std::find(text.begin(), text.end(), std::byte{0});
    if (terminator == text.end())
        throw FormatError("43D5F0 returned unterminated text");
    const auto length = static_cast<std::size_t>(terminator - text.begin()) + 1u;
    std::size_t at = 0;
    for (; at + 4 <= length; at += 4)
        put(bytes, 0x114u + at, get(text, at));
    for (; at < length; ++at)
        bytes[0x114u + at] = text[at];
    for (std::uint32_t car = 0; car < 21; ++car)
    {
        const auto stats = car_statistics_436d60(f, 0x5f2798u + car * 0xecu);
        for (std::uint32_t k = 0; k < 3; ++k)
            bytes[0x18fu + car * 3u + k] = static_cast<std::byte>(stats[k] & 255u);
    }
}
} // namespace
std::uint32_t source_car_class_437cc0(std::uint32_t car)
{
    // 437CC0 LEA EAX,[ECX-1]; CMP EAX,0x14; JA 437CEF (return0).
    // 437CC8 XOR ECX,ECX; 437CCA MOV CL,[437D04+car-1] =0,1,2,2,0,1,1,0,0,1,1,2,1,2,2,0,3,3,3,3,3.
    // 437CD0 JMP [437CF4+index*4]; the pointer table (not listing order) is
    // 437CDD,437CD7,437CE3,437CE9 -> index0=3, index1=1, index2=2, index3=4.
    // Class1 Wild Wheels(lang0x187), 2 Aero Blasters(0x189), 3 Speed Demons(0x188),
    // 4 Team Specials(0x18A): 45189E copies row 4868D8+class*32 (lang id word+12).
    // NEW-port defect r22: the old table read the targets in address order and
    // swapped classes1/3 (Mean Streak listed under Wild Wheels, The Brute under
    // Speed Demons; 44B704 championship series16/18 swapped the same way).
    constexpr std::array<std::uint32_t, 21> classes{3, 1, 2, 2, 3, 1, 1, 3, 3, 1, 1, 2, 1, 2, 2, 3, 4, 4, 4, 4, 4};
    return car - 1u < classes.size() ? classes[car - 1u] : 0u;
}
std::uint32_t create_frontend_car_page_43e000(FrontendState &f, std::uint32_t key, SourceNavigationPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x1d0);
    const auto event = word(f, 0x4869d4);
    const auto bytes = page_object_bytes(f, page, 0x1d0);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto close = word(f, 0x4869d0);
    put(bytes, 4, 8);
    put(bytes, 12, close);
    put(bytes, 16, event);
    put(bytes, 0, 0x47972c);
    put(bytes, 8, 0);
    initialize_car_page_451700(f, page, calls);
    put(bytes, 8, key);
    return page;
}
void destroy_frontend_car_page_children_451a80(FrontendState &f, std::uint32_t page)
{
    const auto bytes = page_object_bytes(f, page, 0x1d0);
    const auto child = get(bytes, 0x14);
    close_menu_text(f, child);
    release_frontend_page_object_43d4c0(page_pool(f), child);
    put(bytes, 0x14, 0);
}
std::uint32_t create_frontend_track_page_43e000(FrontendState &f, std::uint32_t key,
                                                SourceNavigationPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x34);
    const auto bytes = page_object_bytes(f, page, 0x34);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto event = word(f, 0x489514);
    put(bytes, 8, 0);
    const auto close = word(f, 0x489510);
    put(bytes, 12, close);
    put(bytes, 16, event);
    put(bytes, 4, 9);
    put(bytes, 0, 0x47973c);
    const auto count = word(f, 0x5e3968);
    put(bytes, 0x1c, 0);
    source_selection_sort(f, 0x5e396c, count,
                          std::array<std::uint32_t, 24>{19, 1,  16, 7,  18, 17, 21, 8, 12, 13, 14, 5,
                                                        6,  24, 20, 15, 23, 9,  10, 2, 22, 11, 3,  4});
    const auto remembered = word(f, 0x5e39d4);
    if (remembered != 0)
    {
        const auto n = word(f, 0x5e3968);
        for (std::uint32_t i = 0; as_signed(i) < as_signed(n); ++i)
            if (word(f, 0x5e396cu + i * 4u) == remembered)
            {
                put(bytes, 0x1c, i);
                break;
            }
    }
    const auto player = word(f, 0x5e3798);
    put(bytes, 0x28, 0);
    const auto position = integer_float_word(get(bytes, 0x1c));
    put(bytes, 0x18, player);
    const auto laps = word(f, 0x5e39d8);
    put(bytes, 0x2c, 0);
    put(bytes, 0x20, position);
    put(bytes, 0x24, position);
    f.source.source_word(0x5e3964) = 0;
    if (laps == 5)
        put(bytes, 0x30, 1);
    else if (laps == 9)
        put(bytes, 0x30, 2);
    else
    {
        if (laps != 3)
            f.source.source_word(0x5e39d8) = 3;
        put(bytes, 0x30, 0);
    }
    switch (word(f, 0x5e3790))
    {
    case 1:
        f.source.source_word(0x489508) = 0x4893c0;
        break;
    case 2:
        f.source.source_word(0x489508) = 0x4892a0;
        break;
    case 3:
        f.source.source_word(0x489508) = 0x489340;
        break;
    case 5:
        f.source.source_word(0x489508) = 0x489400;
        break;
    case 6:
        f.source.source_word(0x489508) = 0x489440;
        break;
    case 7:
        f.source.source_word(0x489508) = 0x4894a0;
        break;
    default:
        break; // mode4 and values outside1..7 retain source row pointer
    }
    auto child = create_menu_text(f, 0x4894e0, 0x489518, false, calls);
    put(bytes, 0x14, child);
    child = get(bytes, 0x14);
    initialization_event(f, child, 24);
    initialization_event(f, child, 22);
    put(bytes, 8, key);
    return page;
}
void finalize_frontend_track_page_45c670(FrontendState &f, std::uint32_t page)
{
    const auto selection = get(page_object_bytes(f, page, 0x34), 0x30);
    f.source.source_word(0x5e39d8) = selection == 0 ? 3u : selection == 2 ? 9u : 5u;
}
void destroy_frontend_track_page_children_45c640(FrontendState &f, std::uint32_t page)
{
    const auto bytes = page_object_bytes(f, page, 0x34);
    const auto child = get(bytes, 0x14);
    close_menu_text(f, child);
    release_frontend_page_object_43d4c0(page_pool(f), child);
    put(bytes, 0x14, 0);
    finalize_frontend_track_page_45c670(f, page);
}
std::uint32_t create_frontend_start_page_43e000(FrontendState &f, std::uint32_t key,
                                                SourceNavigationPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x1c);
    const auto close = word(f, 0x4879c0);
    const auto bytes = page_object_bytes(f, page, 0x1c);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    put(bytes, 8, 0);
    const auto event = word(f, 0x4879c4);
    put(bytes, 12, close);
    put(bytes, 4, 24);
    put(bytes, 0, 0x4797e4);
    put(bytes, 16, event);
    put(bytes, 0x14, 0);
    put(bytes, 0x18, 0);
    calls.feedback_440880(f, 16); // complete452B30
    put(bytes, 8, key);
    return page;
}
} // namespace sgn::retail
