#include "sgn/source_menu_page_selection.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_options_page.hpp"
#include "sgn/source_video_options_page.hpp"
#include "sgn/host_graphics_page.hpp"
#include "sgn/source_single_page.hpp"
#include "sgn/source_selection_pages.hpp"
#include "sgn/source_save_page.hpp"
#include "sgn/race_session.hpp"
#include "sgn/frontend_page_words.hpp"
#include "sgn/little_endian_words.hpp"
#include <algorithm>
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::little_endian::read_le32;
using sgn::little_endian::write_le32;
using sgn::retail::frontend_page_words::page_pool;
std::uint32_t input_kind(RaceSessionState &world, unsigned slot)
{ // 437090
    if (world.menu_input.raw_words[slot].word_0 == 0)
        return 0;
    const auto kind = world.source_input.physical[slot].activity_kind;
    return kind - 1u <= 4u ? kind : 0;
}
void select_profile(RaceSessionState &world, unsigned slot, std::uint32_t selection)
{ // 437060/45ECF0
    if (input_kind(world, slot) != 0 && selection != 0)
        world.source_input.physical[slot].profile_index = selection - 1u;
}
std::uint32_t feedback_kind(RaceSessionState &world, unsigned slot)
{ // 437010/45ED60
    if (input_kind(world, slot) == 0)
        return 0;
    const auto mode = world.source_input.physical[slot].feedback_mode;
    return mode == 1 || mode == 2 ? mode : 0;
}
} // namespace
void configure_frontend_inputs_43dc40(FrontendState &frontend)
{
    auto &world = frontend.world();
    auto bytes = std::span{frontend.source.persistence.bytes};
    for (unsigned slot = 0; slot < 4; ++slot)
    {
        const auto at = 0xa4u + slot * 12u; // 5DF604+slot*12
        const auto kind = input_kind(world, slot), old = persistence_word(bytes, at);
        if (kind == old)
        {
            select_profile(world, slot, persistence_word(bytes, at + 4));
            // 437040 always returns 1 and has no stores; its EDX argument is ignored.
        }
        else
        {
            if (old != 0)
                select_profile(world, slot, 1); // 437040(1) again has no effects
            persistence_word(bytes, at, kind);
            // 437050 calls437090 and returns zero irrespective of the result.
            (void)input_kind(world, slot);
            persistence_word(bytes, at + 4, 0);
            persistence_word(bytes, at + 8, feedback_kind(world, slot));
        }
    }
}
std::uint32_t create_frontend_title_page_43e000(FrontendState &frontend, std::uint32_t music_key)
{
    const auto page = allocate_frontend_page_object_43d470(page_pool(frontend), 0x24);
    const auto bytes = frontend_page_object_bytes(page_pool(frontend), page, 0x24);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    write_le32(bytes, 8, 0);
    write_le32(bytes, 0x0c, 0x45c070);
    write_le32(bytes, 0x10, 0x45c080);
    write_le32(bytes, 4, 1);
    write_le32(bytes, 0, 0x479670); // PageTitle
    // Full45C030:43DC40 then43C400 then45C050; returns1 unconditionally.
    configure_frontend_inputs_43dc40(frontend);
    copy_persistence_comparison(frontend.source.persistence_view());
    reset_frontend_title_page_45c050(frontend, page);
    write_le32(bytes, 8, music_key); // 43ED94, after successful initializer
    return page;
}
std::uint32_t publish_frontend_page_43ee40(FrontendState &frontend, std::uint32_t page,
                                           SourceNavigationPageServices &calls, CampaignScoreMusic &music)
{
    auto word = [&](std::uint32_t address) -> std::uint32_t & { return frontend.source.source_word(address); };
    const auto pending = word(0x5ff3f8);
    if (pending != 0 && word(0x5ff3fc) == 0)
    {
        calls.close_page(frontend, pending);
        release_frontend_page_object_43d4c0(page_pool(frontend), word(0x5ff3f8)); // reread after close
    }
    const auto phase = std::bit_cast<std::int32_t>(word(0x5ff3e8));
    word(0x5ff3f8) = 0;
    if (phase <= 0 || phase > 3)
        return 0; // source retains pending ownership word here
    const auto page_bytes = frontend_page_object_bytes(page_pool(frontend), page, 12);
    const auto key = read_le32(page_bytes, 8);
    if (key != 0)
    {
        const auto track = word(0x5fcab0u + key * 4u); // full43EEE0, wrapping x86 address arithmetic
        if (track != word(0x5ff4ac))
        {
            music.start(track, 1); // 43DC30 sets EDX1 then tail45F8F0
            word(0x5ff4ac) = track;
        }
    }
    const auto current = word(0x5ff3f0);
    word(0x5ff3f8) = page;
    word(0x5ff3fc) = 0;
    if (current != 0)
        calls.page_event(frontend, current, 20, 0); // 43EEC3 ECX/EDX/stack
    word(0x5ff3e8) = 2;
    return 1;
}
std::uint32_t select_frontend_page_43ef10(FrontendState &frontend, std::uint32_t music_key, std::uint32_t page_kind,
                                          SourcePageFactoryServices &factory, SourceNavigationPageServices &calls,
                                          CampaignScoreMusic &music)
{
    frontend.shared_platform()->require_owner_thread();
    const auto kind = std::bit_cast<std::int32_t>(page_kind);
    if (kind <= 0 || kind >= 41)
        return 0;
    auto word = [&](std::uint32_t address) -> std::uint32_t & { return frontend.source.source_word(address); };
    std::uint32_t i = 0;
    while (std::bit_cast<std::int32_t>(i) < std::bit_cast<std::int32_t>(word(0x5ff440)))
    {
        // New host boundary: valid source stack has16 slots; no bank extension.
        if (i >= 16)
            throw FormatError("frontend page stack exceeds its existing owner");
        const auto address = 0x5ff400u + i * 4u;
        calls.close_page(frontend, word(address));
        release_frontend_page_object_43d4c0(page_pool(frontend), word(address));
        word(address) = 0;
        ++i; // count is reread after every close/release/clear, not snapshotted
    }
    word(0x5ff440) = 0;
    const auto page = factory.create_page_43e000(frontend, music_key, page_kind);
    return publish_frontend_page_43ee40(frontend, page, calls, music); // unconditional, even page0
}
std::uint32_t pop_frontend_page_43eef0(FrontendState &frontend, SourceNavigationPageServices &calls,
                                       CampaignScoreMusic &music)
{
    auto &count = frontend.source.source_word(0x5ff440);
    if (count == 0)
        return 0;
    --count; // publication before reading stack slot
    if (count >= 16)
        throw FormatError("frontend page pop exceeds its existing owner");
    const auto page = frontend.source.source_word(0x5ff400u + count * 4u);
    return publish_frontend_page_43ee40(frontend, page, calls, music); // tail43EE40 result; slot is NOT cleared
}
std::uint32_t SourceMenuPageSelectionChildren::select_page_43ef10(FrontendState &frontend, std::uint32_t key,
                                                                  std::uint32_t kind)
{
    return select_frontend_page_43ef10(frontend, key, kind, *this, *this, music_);
}
std::uint32_t SourceMenuPageSelectionChildren::create_page_43e000(FrontendState &frontend, std::uint32_t key,
                                                                  std::uint32_t kind)
{
    if (kind == 1)
        return create_frontend_title_page_43e000(frontend, key);
    if (kind == 2)
        return create_frontend_main_page_43e000(frontend, key, *this);
    if (kind == 15)
        return create_frontend_options_page_43e000(frontend, key, *this);
    if (kind == 18)
        return create_frontend_video_options_page_43e000(frontend, key, *this);
    if (kind == 3)
        return create_frontend_single_page_43e000(frontend, key, *this);
    if (kind == 10)
        return create_frontend_car_page_43e000(frontend, key, *this);
    if (kind == 11)
        return create_frontend_track_page_43e000(frontend, key, *this);
    if (kind == 25)
        return create_frontend_start_page_43e000(frontend, key, *this);
    if (kind == 36)
        return create_frontend_game_loop_page_43e000(frontend, key);
    if (kind - 1u >= 40u)
        return 0; // 43E00B unsigned factory guard
    return create_other_page(frontend, key, kind);
}
void SourceMenuPageSelectionChildren::close_page(FrontendState &frontend, std::uint32_t page)
{
    const auto bytes = frontend_page_object_bytes(page_pool(frontend), page, 16);
    const auto callback = read_le32(bytes, 12);
    if (callback == 0x45c070)
        return; // entire title destructor is RET; parent frees page
    if (callback == 0x4531d0)
    {
        destroy_frontend_main_page_children_4531d0(frontend, page);
        return;
    }
    if (callback == 0x454d80)
    {
        destroy_frontend_options_page_454d80(frontend, page);
        return;
    }
    if (callback == 0x455690)
    {
        destroy_frontend_video_options_page_455690(frontend, page);
        return;
    }
    if (callback == host_graphics_page_close_r23)
    {
        destroy_host_graphics_page_r23(frontend, page);
        return;
    } // NEW HOST (R23)
    if (callback == 0x45a7f0)
    {
        destroy_frontend_single_page_children_45a7f0(frontend, page);
        return;
    }
    if (callback == 0x451a80)
    {
        destroy_frontend_car_page_children_451a80(frontend, page);
        return;
    }
    if (callback == 0x45c640)
    {
        destroy_frontend_track_page_children_45c640(frontend, page);
        return;
    }
    if (callback == 0x452b50)
        return; // complete Start destructor is RET
    if (callback == 0x452fd0)
        return; // complete PageGameLoop destructor is RET
    if (callback == 0x457ea0)
    {
        destroy_frontend_save_page_457ea0(frontend, page);
        return;
    }
    close_other_page(frontend, page, callback);
}
} // namespace sgn::retail
