#include "sgn/source_menu_update.hpp"
#include <bit>

namespace sgn::retail
{
bool source_menu_transition_ready_43efc0(const FrontendSourceStorage &source) noexcept
{
    // 43EFC0..43EFE4: LEA/LEA/SHL wraps before CDQ and signed /65536.
    const auto product = source.navigation_words[1] * std::uint32_t{100};
    return std::bit_cast<std::int32_t>(product) / 65536 > 95;
}
std::uint32_t select_source_menu_language_452d70(FrontendState &frontend, std::uint32_t flag,
                                                 SourceMenuUpdateChildren &calls)
{
    // 452D71 keeps ECX in ESI across 440C40; EAX is tested signed.
    if (calls.select_language_440c40(frontend, flag) >= 0)
        persistence_word(frontend.source.persistence.bytes, 0x90, flag); // 452D7C
    return 1; // 452D82, including a negative result from the language child
}
void update_source_menu_43d200(FrontendState &frontend, SourceMenuUpdateChildren &calls)
{
    calls.update_words_436ed0(frontend);      // 43D200
    calls.update_navigation_43eff0(frontend); // 43D205
    // 43D20A calls 437EB0 (JMP 441210), whose destination is one-byte RET.
    calls.apply_options_43c000(frontend); // 43D20F
    auto &source = frontend.source;
    const auto requested = persistence_word(source.persistence.bytes, 0x90); // 43D214
    if (requested == 0 || requested == source.source_word(0x5e37b8))
        return;
    if (!source_menu_transition_ready_43efc0(source))
        return; // 43D225
    // 43D22E reloads ECX; the selected catalog word is read after 452D70.
    if (select_source_menu_language_452d70(frontend, persistence_word(source.persistence.bytes, 0x90), calls) == 0)
        persistence_word(source.persistence.bytes, 0x90, source.source_word(0x5e37b8)); // 43D23D..242
}
void draw_source_menu_43d250(FrontendState &frontend, SourceMenuDrawChildren &calls)
{
    calls.draw_pages_43f6b0(frontend);   // 43D250
    calls.submit_lists_437ec0(frontend); // 43D255 tail
}
} // namespace sgn::retail
