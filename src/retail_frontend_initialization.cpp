#include "sgn/retail_frontend_initialization.hpp"
#include "retail_frontend_navigation_stores.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include "sgn/retail_frontend_launch.hpp"
#include "sgn/retail_frontend_page_pool.hpp"

namespace sgn::retail
{
namespace
{
// These are the complete pinned D3D leaves, not substitute host backends.
// EXE SHA256: 4a1d234ce1f095a427bb7f2f0174294b8c85a8fdbbc29b62a3863156b2f16a1b.
constexpr std::uint32_t source_462260() noexcept
{
    return 1;
} // 462260..462265
void source_462270(std::uint32_t /*ecx*/) noexcept {} // 462270: RET
constexpr std::uint32_t source_424a80() noexcept
{
    return 2;
} // 424A80..424A85
void source_424a90(std::uint32_t /*ecx*/) noexcept {} // 424A90: RET
} // namespace

void initialize_frontend_defaults_43c060(FrontendSourceStorage &frontend, const FrontendLaunchState &world)
{
    static_assert(persistence_size == 0x847 * 4);
    auto live = frontend.persistence_view().live;
    frontend.persistence.bytes.fill(0); // 43C062..43C070, exactly 0x847 words
    const auto put = [&](std::uint32_t address, std::uint32_t value) {
        persistence_word(live, address - FrontendSourceStorage::cold_base, value);
    };
    // 43C072..43C0D1: preserve the source store order, including explicit zeros.
    put(0x5df634, 0);
    put(0x5df5e0, 100);
    put(0x5df5e4, 100);
    put(0x5df638, 0);
    put(0x5df63c, 0);
    put(0x5df640, 0);
    put(0x5df644, 0);
    put(0x5df648, 0);
    put(0x5df5d0, 0);
    put(0x5df5d4, 0);
    put(0x5df5d8, 1);
    put(0x5df5dc, 0x50);
    put(0x5df5e8, 0);
    put(0x5df5ec, 0);
    put(0x5df5f0, 0);
    // 43C0D7..43C0EC: five triplets, 5DF5F8..5DF630 inclusive.
    for (std::uint32_t address = 0x5df5fc; address < 0x5df638; address += 12)
    {
        put(address - 4, 0);
        put(address, 0);
        put(address + 4, 0);
    }
    // 43C0EE..43C0F3 reads the current sole 60DF90 word after the clears.
    frontend.source_word(0x5f26a8) = world.catalog_count;
    auto setting = source_462260();           // 43C0F8
    frontend.source_word(0x5e37a0) = setting; // 43C0FF
    if (setting == 0)
    { // 43C106..43C10D; unreachable with this pinned leaf
        setting = 1;
        frontend.source_word(0x5e37a0) = setting;
    }
    source_462270(setting);                       // 43C112..43C114, ECX=1
    const auto display_setting = source_424a80(); // 43C119
    put(0x5df5f4, display_setting);               // 43C11E
    source_424a90(display_setting);               // 43C123..43C125, ECX=2, source tail call
}

void clear_frontend_race_return_439e30(FrontendSourceStorage &frontend) noexcept
{
    // 439E30..439E40: REP STOSD clears13 words,5DF528..5DF55B.
    // The pointer-word map is also visible in439E90's six populated switch slots.
    frontend.race_return_selector = 0; // 5DF528, first source word
    for (const auto slot : {1u, 3u, 4u, 5u, 6u, 7u})
    {
        frontend.race_return_bindings[slot].roster = {}; // 5DF52C/534/53C/544/54C/554
        frontend.race_return_bindings[slot].result = {}; // 5DF530/538/540/548/550/558
    }
}

void initialize_frontend_feedback_gate_4408e0(FrontendSourceStorage &frontend) noexcept
{
    frontend.feedback_gate = 1; // 4408E0..4408EA: MOV [5FF8C0],1;RET
}
void increment_frontend_feedback_gate_440900(FrontendSourceStorage &frontend) noexcept
{
    ++frontend.feedback_gate; // 440900..440906: INC [5FF8C0];RET, modulo32
}
void decrement_frontend_feedback_gate_440910(FrontendSourceStorage &frontend) noexcept
{
    --frontend.feedback_gate; // 440910..440916: DEC [5FF8C0];RET, modulo32
}

void clear_frontend_draw_lists_437e80(FrontendSourceStorage &frontend) noexcept
{
    frontend.menu_counter_5da370 = 0; // 437E80..437E87
    // 437E8D..437E9D: EAX=5DF2D4+4*i, writes EAX-140 then EAX.
    // One contiguous owner, with the two80-word source ranges interleaved.
    for (std::size_t i = 0; i < 80; ++i)
    {
        frontend.menu_words_5df194[i] = 0;
        frontend.menu_words_5df194[80 + i] = 0;
    }
}

void clear_frontend_words_436eb0(FrontendSourceStorage &frontend) noexcept
{
    frontend.menu_words_5da2e8.fill(0); // 436EB1..436EBD: ECX30,EAX0,EDI5DA2E8,REP STOSD
}

void clear_frontend_navigation_tail_43dee0(FrontendSourceStorage &frontend) noexcept
{
    detail::frontend_navigation_tail_stores_43dee0([&](std::uint32_t address, std::uint32_t value) noexcept {
        frontend.navigation_words[(address - 0x5ff3e8) / 4] = value;
    });
}
void initialize_frontend_navigation_43dda0(FrontendSourceStorage &frontend) noexcept
{
    detail::frontend_navigation_stores_43dda0([&](std::uint32_t address, std::uint32_t value) noexcept {
        frontend.navigation_words[(address - 0x5ff3e8) / 4] = value;
    });
}

void initialize_frontend_drawing_437e60(FrontendSourceStorage &frontend)
{
    clear_frontend_draw_lists_437e80(frontend);                                       // 437E60: actual first child
    initialize_frontend_materials_441190(frontend, source_world_material_prelude(2)); // 437E65 returns to437E6A
    set_frontend_clip_438190(frontend, {0u, 0u, 0x44200000u, 0x43f00000u});           // 437E6A..437E7D
}

FrontendInitializationStop initialize_frontend_prefix_43d0b0(FrontendSourceStorage &frontend,
                                                             const FrontendLaunchState &world)
{
    const auto next_count = frontend.initialization_count + std::uint32_t{1}; // 43D0B0..43D0BC
    frontend.menu_state = 0;                                                  // 43D0C3
    frontend.initializing = 1;                                                // 43D0CD
    frontend.initialization_count = next_count;                               // 43D0D3, modulo32
    // 43D0BE/43D0D9: ECX=4795EC, call437E40. Its complete body is RET;
    // it dereferences no object and performs no observable source operation.
    initialize_frontend_page_pool_43d430(std::as_writable_bytes(std::span{frontend.page_pool})); // 43D0DE
    clear_frontend_cold_storage(frontend);                // 43D0E3..43D0F0, 5DF560..5FD047 only
    initialize_frontend_defaults_43c060(frontend, world); // 43D0F1..43D0F5
    clear_frontend_race_return_439e30(frontend);          // 43D0F6..43D0FA
    initialize_frontend_feedback_gate_4408e0(frontend);   // 43D0FB..43D0FF
    initialize_frontend_drawing_437e60(frontend);         // 43D100 completes and returns
    clear_frontend_words_436eb0(frontend);                // 43D105 completes436EB0
    initialize_frontend_navigation_43dda0(frontend);      // 43D10A including source tail43DEE0
    return FrontendInitializationStop::before_440ae0;     // stop before43D10F
}
} // namespace sgn::retail
