#include "sgn/source_start_mode2.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_outer_scheduler.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/retail_pause_music.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
Word word(FrontendState &f, Word address)
{
    const auto b = f.source.source_bytes(address, 4);
    Word v = 0;
    for (Word i = 0; i < 4; ++i)
        v |= Word(b[i]) << (i * 8u);
    return v;
}
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
template <std::size_t N> std::span<const std::byte, N> bank(FrontendState &f, Word address)
{
    const auto bytes = std::as_bytes(f.source.source_bytes(address, N));
    return std::span<const std::byte, N>{bytes.data(), N};
}
void settings_to_race_439e50(FrontendState &f)
{
    const auto a = word(f, 0x5df5e0), c = word(f, 0x5df5dc), d = word(f, 0x5df5e4);
    auto &options = f.world().options;
    options.audio_b = a;
    const auto display = word(f, 0x5df5d0);
    options.audio_a = c;
    options.render_a = d;
    options.display_selector = display;
    configure_frontend_inputs_43dc40(f); // 439E7C tail
}
void enter_race_439e10(FrontendState &f, FrontendPerformanceCounter &counter)
{
    // 43D420 before and after is exactly XOR EAX,EAX;RET.
    auto transition = scene_transition_world(f.world());
    auto results = scene_race_result_world(f.world());
    request_scene_transition(transition, results);                     // 439E15 -> full441E20
    reset_platform_source_clock_442100(*f.shared_platform(), counter); // 439E1A, two real counter reads
}
} // namespace
void prepare_frontend_stock_configuration_43a430(FrontendState &f, SelectionRecord &selected, Word model, Word livery)
{
    std::array<std::byte, 0x218> scratch;
    compose_campaign_configuration_436490({bank<0x57e4>(f, 0x5f3af4), bank<0x660>(f, 0x5f943c)}, 0,
                                          bank<0xec>(f, 0x5f26acu + model * 0xecu),
                                          bank<300>(f, 0x5f3af4u + model * 300u), scratch);
    publish_frontend_vehicle_configuration(selected, std::span<const std::byte, 300>{scratch.data() + 0xec, 300}, model,
                                           livery);
}
void prepare_frontend_mode2_race_43a780(FrontendState &f, Word roster)
{
    f.shared_platform()->require_owner_thread();
    f.source.race_return_selector = 2; // 43A78E; source default return slot, no fabricated result destination
    const auto initial_count = word(f, roster + 0xa8u);
    auto view = frontend_launch_world(f);
    clear_frontend_race_selection(view); // 43D420 RET then43A720
    auto &world = f.world();
    auto &options = world.options;
    auto &launch = world.frontend_launch;
    const auto track = word(f, roster + 0xacu);
    options.render_b = 0;
    options.split = 0;
    options.game_type = 2;
    const auto music = word(f, 0x5fcb78u + track * 24u), texture = word(f, 0x5fcb80u + track * 24u);
    launch.track_index = track - 1u;
    const auto laps = word(f, roster + 0xb0u);
    options.texture = texture;
    options.music = music;
    options.laps = laps;
    options.race_mode = 1;
    options.start_delay = 4;
    options.demo_override = 0;
    launch.prepared = 1;
    options.record = 0x1c20;
    options.record_f004 = 0x8ca0;
    options.record_f008 = 0;
    options.record_f00c = 0;
    if (std::bit_cast<std::int32_t>(initial_count) <= 0)
        return; // 43A902/43D420 literal RET path
    Word row = 0;
    do
    {
        const auto at = roster + row * 0x1cu + 4u, grid = word(f, at + 8u) - 1u;
        if (grid >= world.selection.size())
            throw FormatError("43A780 grid leaves the six source selection owners");
        auto &selected = world.selection[grid];
        // 43D690 copies3 bytes to the existing47966C scratch; fourth byte is retained.
        const auto initials = f.source.source_bytes(at, 3);
        std::copy_n(initials.begin(), 3, f.source.initials_scratch.begin());
        const auto end = std::find(f.source.initials_scratch.begin(), f.source.initials_scratch.end(), 0);
        if (end == f.source.initials_scratch.end())
            throw FormatError("43D690 scratch string leaves its existing owner");
        const auto length = static_cast<std::size_t>(end - f.source.initials_scratch.begin()) + 1u;
        std::memcpy(selected.name.data(), f.source.initials_scratch.data(), length);
        selected.kind = 2;
        if (word(f, at - 4u) != 0)
        {
            const auto controller = word(f, 0x5e37a4);
            selected.control = 0;
            bind_frontend_race_input(launch, controller, grid, 1);
            world.trial_selection.split_selection = grid;
        }
        else
        {
            const auto skill = word(f, at + 0x14u), mode = word(f, at + 0x10u);
            selected.control = 1;
            selected.ai_skill = skill;
            selected.ai_mode = mode;
        }
        const auto livery = word(f, at + 0xcu), model = word(f, at + 4u);
        prepare_frontend_stock_configuration_43a430(f, selected, model, livery);
        ++row;
    } while (std::bit_cast<std::int32_t>(row) < std::bit_cast<std::int32_t>(word(f, roster + 0xa8u)));
    // 43A902 ->43D420 is literal RET; no platform dispatch or world launch here.
}
void request_frontend_save_page_43d500(FrontendState &f, Word key, SourcePageFactoryServices &factory,
                                       SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    if (!persistence_changed(f.source.persistence_view()))
        return; // full43C430 condition
    const auto card = word(f, 0x5df5d8);
    f.source.source_word(0x5e3c1c) = card;
    f.source.source_word(0x5e3c20) = 1;
    f.source.source_word(0x5e3c28) = 3;
    f.source.source_word(0x5e3c24) = 0;
    (void)push_frontend_page_43dfa0(f, key, 27, factory, pages, music); // source save page, not a synthetic save
}
Word dispatch_frontend_start_page_452b60(FrontendState &f, Word page, Word event, SourceNavigationPageServices &pages,
                                         CampaignScoreMusic &music, FrontendPerformanceCounter &counter)
{
    f.shared_platform()->require_owner_thread();
    if (event == 19)
    {
        f.source.source_word(0x5ff46c) = 0;
        set_frontend_packed_word_43dd10(f.source, 0);
        f.source.source_word(0x5ff448) = 0;
        f.source.source_word(0x5ff44c) = 0;
        f.source.source_word(0x5ff450) = 0;
        if (page_word(f, page, 0x14) == 0)
        {
            auto &world = f.world();
            set_music_words_45f8e0(world.audio_stream.field_e100, world.audio_maintenance.music.gain_step, 0,
                                   0xfffffffcu);
            set_page_word(f, page, 0x14, 1);
        }
    }
    else if (event == 1)
    {
        const auto state = page_word(f, page, 0x14);
        if (state == 1)
        {
            const auto old = page_word(f, page, 0x18);
            set_page_word(f, page, 0x18, old + 1u);
            if (std::bit_cast<std::int32_t>(old) >= 120)
            {
                settings_to_race_439e50(f);
                enter_race_439e10(f, counter);
                set_page_word(f, page, 0x14, 2);
            }
        }
        else if (state == 2)
        {
            auto result = frontend_race_return_world(f);
            return_frontend_race_result(result);
            clear_frontend_words_436eb0(f.source);
            initialize_frontend_lookup_tables(f.source); // full4411C0
            set_page_word(f, page, 0x14, 3);
            f.source.source_word(0x5ff4ac) = 0; // full43DE40
            (void)pop_frontend_page_43eef0(f, pages, music);
        }
    }
    return 0;
}
} // namespace sgn::retail
