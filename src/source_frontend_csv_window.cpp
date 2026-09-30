#include "sgn/source_frontend_csv_window.hpp"
#include "sgn/source_frontend_csv.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <string_view>

namespace sgn::retail
{
std::uint32_t parse_frontend_csv_434e90(SourceFrontendCsv &csv)
{
    // 478CAC/478CB4/478CBC and478CC4: Easy, Medium, Hard; Car%02d%s.
    constexpr std::uint32_t difficulties[]{0x00478cacu, 0x00478cb4u, 0x00478cbcu};
    csv.next_line_43c5c0(); // 434EB2
    std::uint32_t destination = 0x005fcdf0u;
    for (const auto difficulty : difficulties)
    {
        for (std::uint32_t car = 1; car <= 4; ++car)
        {
            std::uint8_t label[64]; // source local buffer
            const std::uint32_t arguments[]{car, difficulty};
            (void)csv.format_key(0x00478cc4u, arguments, label, 0x00434ee3u);
            (void)csv.label_43c600(
                std::string_view{reinterpret_cast<const char *>(label)}); // 434EEC; source ignores return
            const auto token_address = destination - 8u;                  // 434EF3..434EF7
            // Borrow the remaining actual cold bank, not a second row buffer.
            // Retail token child is not capped at8: preserve writes into later
            // fields/rows. Crossing the known owner is a NEW host bounds error.
            constexpr auto cold_end = FrontendSourceStorage::cold_base + FrontendSourceStorage::cold_bytes;
            (void)csv.token_43c680(csv.frontend.source.source_bytes(token_address, cold_end - token_address));
            (void)csv.number_43c770(csv.frontend.source.source_word(destination));       // 434F01
            (void)csv.number_43c770(csv.frontend.source.source_word(destination + 4u));  // 434F0C
            (void)csv.number_43c770(csv.frontend.source.source_word(destination + 8u));  // 434F17
            (void)csv.number_43c770(csv.frontend.source.source_word(destination + 12u)); // 434F22
            csv.next_line_43c5c0(); // 434F2B, even after child failure
            destination += 0x18u;   // 434F33; four rows per difficulty,12 total
        }
    }
    return 1u; // 434F52
}
std::uint32_t parse_frontend_csv_434e10(SourceFrontendCsv &csv)
{
    csv.next_line_43c5c0(); // 434E1D
    std::uint32_t destination = 0x005fcfc0u;
    for (std::uint32_t position = 1; position <= 6; ++position)
    {
        std::uint8_t label[64]; // source local buffer
        const std::uint32_t arguments[]{position};
        (void)csv.format_key(0x00478ca0u, arguments, label, 0x00434e3cu);
        // R25: clang-format off keeps the address comment on one line with its statement (no reflow).
        // clang-format off
        (void)csv.label_43c600(std::string_view{reinterpret_cast<const char *>(label)}); // 434E45; source ignores return
        // clang-format on
        (void)csv.number_43c770(csv.frontend.source.source_word(destination - 4u)); // 434E50
        (void)csv.number_43c770(csv.frontend.source.source_word(destination));      // 434E5A
        csv.next_line_43c5c0();                                                     // 434E63
        destination += 8u; // 434E6B..434E74, last pair5FCFE4/5FCFE8
    }
    return 1u; // 434E78
}
} // namespace sgn::retail
