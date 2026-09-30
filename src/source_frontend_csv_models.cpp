#include "sgn/source_frontend_csv_models.hpp"
#include "sgn/source_frontend_csv.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/source_selection_pages.hpp"
#include <algorithm>
#include <array>
#include <initializer_list>
namespace sgn::retail
{
namespace
{
std::uint32_t &word(SourceFrontendCsv &csv, std::uint32_t address)
{
    return csv.frontend.source.source_word(address);
}
std::string_view text(std::span<const std::uint8_t> bytes)
{
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
    if (end == bytes.end())
        throw FormatError("CSV label lacks a terminator within its actual buffer");
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<std::size_t>(end - bytes.begin())};
}
void key(SourceFrontendCsv &csv, std::uint32_t format, std::initializer_list<std::uint32_t> args,
         std::uint32_t return_site)
{
    std::array<std::uint8_t, 64> output;
    // Host-only poison of source stack scratch. A complete formatter must write
    // its real NUL; uninitialized padding must never invent an empty label.
    output.fill(0xcc);
    (void)csv.format_key(format, {args.begin(), args.size()}, output, return_site);
    (void)csv.label_43c600(text(output)); // Source callers ignore label return.
}
void literal_label(SourceFrontendCsv &csv, std::uint32_t address)
{
    (void)csv.label_43c600(text(csv.literal(address)));
}
void opponent(SourceFrontendCsv &csv, std::uint32_t address)
{
    // 43C680 writes directly at the source name address, not into a name copy.
    // Bound by the real suffix owner rather than inventing an8-byte truncation;
    // a longer token can overwrite adjacent fields exactly before later stores.
    constexpr auto end = FrontendSourceStorage::cold_base + FrontendSourceStorage::cold_bytes;
    if (address >= end)
        throw FormatError("CSV opponent destination outside frontend suffix");
    (void)csv.token_43c680(csv.frontend.source.source_bytes(address, end - address));
    (void)csv.number_43c770(word(csv, address + 8));
    (void)csv.number_43c770(word(csv, address + 12));
    (void)csv.number_43c770(word(csv, address + 16));
    (void)csv.number_43c770(word(csv, address + 20));
}
void setup_component_435f60(SourceFrontendCsv &csv, std::uint32_t destination)
{
    // 435F60 overwrites ECX before use; the apparent label parameter is unread.
    word(csv, destination) = 0; // 435F74
    (void)csv.number_43c770(word(csv, destination + 4));
    (void)csv.number_43c770(word(csv, destination + 8));
    (void)csv.number_43c770(word(csv, destination + 12));
}
void dynamics_row_436010(SourceFrontendCsv &csv, std::uint32_t destination)
{
    (void)csv.number_43c770(word(csv, destination + 0x0));     // 43602B
    (void)csv.floating_43c7b0(word(csv, destination + 0x4));   // 436036
    (void)csv.floating_43c7b0(word(csv, destination + 0x8));   // 436041
    (void)csv.floating_43c7b0(word(csv, destination + 0x104)); // 43604F
    (void)csv.floating_43c7b0(word(csv, destination + 0x108)); // 43605D
    (void)csv.floating_43c7b0(word(csv, destination + 0x10c)); // 43606B
    (void)csv.floating_43c7b0(word(csv, destination + 0x110)); // 436079
    (void)csv.floating_43c7b0(word(csv, destination + 0x114)); // 436087
    (void)csv.floating_43c7b0(word(csv, destination + 0x118)); // 436095
    (void)csv.floating_43c7b0(word(csv, destination + 0xc));   // 4360A0
    (void)csv.floating_43c7b0(word(csv, destination + 0x10));  // 4360AB
    (void)csv.floating_43c7b0(word(csv, destination + 0x14));  // 4360B6
    (void)csv.floating_43c7b0(word(csv, destination + 0x70));  // 4360C1
    (void)csv.floating_43c7b0(word(csv, destination + 0x18));  // 4360CC
    (void)csv.floating_43c7b0(word(csv, destination + 0x1c));  // 4360D7
    (void)csv.floating_43c7b0(word(csv, destination + 0x20));  // 4360E2
    (void)csv.floating_43c7b0(word(csv, destination + 0x24));  // 4360ED
    (void)csv.floating_43c7b0(word(csv, destination + 0x28));  // 4360F8
    (void)csv.number_43c770(word(csv, destination + 0x2c));    // 436103
    (void)csv.number_43c770(word(csv, destination + 0x30));    // 43610E
    (void)csv.floating_43c7b0(word(csv, destination + 0x40));  // 436119
    (void)csv.number_43c770(word(csv, destination + 0x34));    // 436124
    (void)csv.floating_43c7b0(word(csv, destination + 0x38));  // 43612F
    (void)csv.floating_43c7b0(word(csv, destination + 0x3c));  // 43613A
    (void)csv.floating_43c7b0(word(csv, destination + 0x44));  // 436145
    (void)csv.floating_43c7b0(word(csv, destination + 0x48));  // 436150
    (void)csv.floating_43c7b0(word(csv, destination + 0x4c));  // 43615B
    (void)csv.floating_43c7b0(word(csv, destination + 0x50));  // 436166
    (void)csv.floating_43c7b0(word(csv, destination + 0x54));  // 436171
    (void)csv.floating_43c7b0(word(csv, destination + 0x58));  // 43617C
    (void)csv.floating_43c7b0(word(csv, destination + 0x5c));  // 436187
    (void)csv.floating_43c7b0(word(csv, destination + 0x60));  // 436192
    (void)csv.floating_43c7b0(word(csv, destination + 0x64));  // 43619D
    (void)csv.floating_43c7b0(word(csv, destination + 0x68));  // 4361A8
    (void)csv.floating_43c7b0(word(csv, destination + 0x6c));  // 4361B3
    (void)csv.floating_43c7b0(word(csv, destination + 0x11c)); // 4361C1
    (void)csv.floating_43c7b0(word(csv, destination + 0x120)); // 4361CF
    (void)csv.floating_43c7b0(word(csv, destination + 0x74));  // 4361DA
    (void)csv.floating_43c7b0(word(csv, destination + 0x78));  // 4361E5
    (void)csv.floating_43c7b0(word(csv, destination + 0x7c));  // 4361F0
    (void)csv.floating_43c7b0(word(csv, destination + 0x80));  // 4361FE
    (void)csv.floating_43c7b0(word(csv, destination + 0x84));  // 43620C
    (void)csv.floating_43c7b0(word(csv, destination + 0x88));  // 43621A
    (void)csv.number_43c770(word(csv, destination + 0x8c));    // 436228
    (void)csv.number_43c770(word(csv, destination + 0x90));    // 436236
    (void)csv.floating_43c7b0(word(csv, destination + 0x94));  // 436244
    (void)csv.floating_43c7b0(word(csv, destination + 0x98));  // 436252
    (void)csv.floating_43c7b0(word(csv, destination + 0x9c));  // 436260
    (void)csv.floating_43c7b0(word(csv, destination + 0xa0));  // 43626E
    (void)csv.floating_43c7b0(word(csv, destination + 0xa4));  // 43627C
    (void)csv.floating_43c7b0(word(csv, destination + 0xa8));  // 43628A
    (void)csv.floating_43c7b0(word(csv, destination + 0xac));  // 436298
    (void)csv.floating_43c7b0(word(csv, destination + 0xb0));  // 4362A6
    (void)csv.floating_43c7b0(word(csv, destination + 0xb4));  // 4362B4
    (void)csv.floating_43c7b0(word(csv, destination + 0xb8));  // 4362C2
    (void)csv.floating_43c7b0(word(csv, destination + 0x100)); // 4362D0
    (void)csv.floating_43c7b0(word(csv, destination + 0xbc));  // 4362DE
    (void)csv.floating_43c7b0(word(csv, destination + 0xc0));  // 4362EC
    (void)csv.floating_43c7b0(word(csv, destination + 0xc4));  // 4362FA
    (void)csv.floating_43c7b0(word(csv, destination + 0xc8));  // 436308
    (void)csv.floating_43c7b0(word(csv, destination + 0xcc));  // 436316
    (void)csv.floating_43c7b0(word(csv, destination + 0xd0));  // 436324
    (void)csv.number_43c770(word(csv, destination + 0xe8));    // 436332
    (void)csv.number_43c770(word(csv, destination + 0xec));    // 436340
    (void)csv.number_43c770(word(csv, destination + 0xf0));    // 43634E
    (void)csv.number_43c770(word(csv, destination + 0xf4));    // 43635C
    (void)csv.floating_43c7b0(word(csv, destination + 0xd4));  // 43636A
    (void)csv.floating_43c7b0(word(csv, destination + 0xd8));  // 436378
    (void)csv.floating_43c7b0(word(csv, destination + 0xdc));  // 436386
    (void)csv.floating_43c7b0(word(csv, destination + 0xe0));  // 436394
    (void)csv.floating_43c7b0(word(csv, destination + 0xe4));  // 4363A2
    (void)csv.number_43c770(word(csv, destination + 0xf8));    // 4363B0
    (void)csv.number_43c770(word(csv, destination + 0xfc));    // 4363BE
    (void)csv.number_43c770(word(csv, destination + 0x124));   // 4363CC
    (void)csv.floating_43c7b0(word(csv, destination + 0x128)); // 4363DA
    (void)csv.skip_43c520();                                   // 4363E3
}
} // namespace
std::uint32_t parse_frontend_csv_435e30(SourceFrontendCsv &csv, std::uint32_t car, std::uint32_t destination)
{
    for (unsigned offset = 0; offset < 0xec; offset += 4)
        word(csv, destination + offset) = 0; // 435E46,59DWORDs
    key(csv, 0x479248, {car}, 0x435e5a);     // Car%02d
    (void)csv.number_43c770(word(csv, destination + 4));
    (void)csv.number_43c770(word(csv, destination + 8));
    // Exact call order435E8E..435F36, not ascending record order.
    setup_component_435f60(csv, destination + 0x5c);
    setup_component_435f60(csv, destination + 0x4c);
    setup_component_435f60(csv, destination + 0xcc);
    setup_component_435f60(csv, destination + 0x8c);
    setup_component_435f60(csv, destination + 0x3c);
    setup_component_435f60(csv, destination + 0x6c);
    setup_component_435f60(csv, destination + 0xdc);
    setup_component_435f60(csv, destination + 0x9c);
    setup_component_435f60(csv, destination + 0xbc);
    setup_component_435f60(csv, destination + 0x7c);
    setup_component_435f60(csv, destination + 0xac);
    (void)source_car_class_437cc0(car); // 435F3E: existing Mechanic leaf, result ignored
    (void)csv.skip_43c520();            // 435F47
    return 1;
}
std::uint32_t parse_frontend_csv_435fa0(SourceFrontendCsv &csv, std::uint32_t prefix, std::uint32_t destination,
                                        std::int32_t count)
{
    // 435FB0 signedJLE: no format/string/input/destination access for count<=0.
    for (std::int32_t index = 0; index < count; ++index)
    {
        key(csv, 0x4792a8, {prefix, static_cast<std::uint32_t>(index) + 1u}, 0x435fc9); //%s%02d ->436021 label
        dynamics_row_436010(csv, destination);
        destination += 0x12c; // 435FE3,32-bit effective-address arithmetic
    }
    return 1;
}
std::uint32_t parse_frontend_csv_435980(SourceFrontendCsv &csv)
{
    csv.next_line_43c5c0(); // 43598D
    for (unsigned i = 1; i <= 6; ++i)
    {
        key(csv, 0x479184, {i}, 0x4359ac);
        (void)csv.number_43c770(word(csv, 0x5f9bec + (i - 1) * 4));
        csv.next_line_43c5c0(); // 4359C8
    }
    csv.next_line_43c5c0();       // 4359DF
    literal_label(csv, 0x479190); // Gold
    (void)csv.number_43c770(word(csv, 0x5f9c04));
    csv.next_line_43c5c0();       // 435A02
    literal_label(csv, 0x479198); // Silver
    (void)csv.number_43c770(word(csv, 0x5f9c08));
    csv.next_line_43c5c0(); // 435A25
    csv.next_line_43c5c0(); // 435A2E, separate reached call
    for (unsigned i = 1; i <= 18; ++i)
    {
        const std::uint32_t base = 0x5f9c90 + (i - 1) * 0x84;
        key(csv, 0x4791a0, {i}, 0x435a51);
        (void)csv.number_43c770(word(csv, base));
        (void)csv.number_43c770(word(csv, base + 8));
        (void)csv.number_43c770(word(csv, base + 4)); // 435A7A, reordered destination
        csv.next_line_43c5c0();
    }
    csv.next_line_43c5c0(); // 435AA1
    for (unsigned node = 1; node <= 18; ++node)
    {
        std::uint32_t destination = 0x5f9c9c + (node - 1) * 0x84;
        for (unsigned car = 1; car <= 5; ++car)
        {
            key(csv, 0x4791a8, {node, car}, 0x435ad0);
            opponent(csv, destination); // 435AE4..435B0F
            csv.next_line_43c5c0();     // 435B18
            destination += 0x18;
        }
    }
    return 1;
}
std::uint32_t parse_frontend_csv_435860(SourceFrontendCsv &csv)
{
    std::uint32_t destination = 0x5fa880;
    for (unsigned selected_car = 1; selected_car <= 21; ++selected_car)
    {
        key(csv, 0x479170, {selected_car}, 0x4358a0);
        csv.next_line_43c5c0(); // 4358B2
        for (const auto difficulty : {0x479158u, 0x479160u, 0x479168u})
        {
            for (unsigned car = 1; car <= 5; ++car)
            {
                key(csv, 0x479178, {difficulty, car}, 0x4358e5);
                opponent(csv, destination); // 4358F9..435924
                csv.next_line_43c5c0();     // 43592D
                destination += 0x18;
            }
        }
    }
    return 1;
}
} // namespace sgn::retail
