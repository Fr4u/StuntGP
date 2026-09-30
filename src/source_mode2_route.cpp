#include "sgn/source_mode2_route.hpp"
#include "sgn/race_session.hpp"
#include <bit>
namespace sgn::retail
{
void advance_mode2_44cd70(FrontendState &f, SourcePageFactoryServices &factory, SourceNavigationPageServices &pages,
                          CampaignScoreMusic &music, SourceMode2RouteServices &calls)
{
    f.shared_platform()->require_owner_thread();
    auto word = [&](std::uint32_t address) -> std::uint32_t & { return f.source.source_word(address); };
    auto push = [&](std::uint32_t key, std::uint32_t kind) {
        (void)push_frontend_page_43dfa0(f, key, kind, factory, pages, music);
    };
    switch (word(0x5e3794))
    {
    case 0:
        word(0x5e379c) = 1;
        word(0x5e3794) = 1;
        word(0x5e3798) = 0;
        word(0x5e39d4) = 0;
        word(0x5e39dc) = 3;
        word(0x5e39e0) = 0;
        word(0x5e3954) = 0;
        return;
    case 1:
        word(0x5e38f8) = 0;
        for (std::uint32_t car = 1; car < 21; ++car)
        {
            const auto admitted = calls.car_available_437230(f, car, word(0x5e3790));
            if (admitted != 0 || persistence_word(f.source.persistence.bytes, 0x74) != 0)
            {
                const auto index = word(0x5e38f8);
                word(0x5e38fc + index * 4u) = car;
                ++word(0x5e38f8);
            }
        }
        word(0x5e3794) = 2;
        push(33, 10);
        return;
    case 2:
        if (word(0x5e38f4) != 0)
        {
            (void)pop_frontend_page_43eef0(f, pages, music);
            return;
        }
        word(0x5e3968) = 0;
        for (std::uint32_t track = 1; track < 25; ++track)
        {
            const auto admitted = calls.track_available_4371c0(f, track, word(0x5e3790));
            if (admitted != 0 || persistence_word(f.source.persistence.bytes, 0x74) != 0)
            {
                const auto index = word(0x5e3968);
                word(0x5e396c + index * 4u) = track;
                ++word(0x5e3968);
            }
        }
        word(0x5e3794) = 3;
        push(34, 11);
        return;
    case 3: {
        if (word(0x5e3964) != 0)
        {
            word(0x5e3794) = 1;
            return;
        }
        // Capture ECX/EDX/EAX before publication or formatting callbacks.
        const auto count = word(0x5e39dc) + 1u;
        const auto config0 = word(0x5e39d4), config1 = word(0x5e39d8);
        word(0x61bff8) = count;
        word(0x61bffc) = config0;
        word(0x61c000) = config1;
        const auto format = frontend_language_text_441170(f.source.language_strings(), 31);
        std::array<std::uint8_t, 64> name; // source local is initialized only by46280C
        const auto written = calls.format_name_46280c(f, format, 1, name);
        if (written < 3u || written > name.size())
            throw FormatError("44CD70 name local has no defined three-byte prefix");
        auto rank = word(0x61bff8);
        const auto selected = word(0x5e3954 + word(0x5e3798) * 4u);
        calls.rank_car_437cc0(f, selected);
        auto initials = f.source.source_bytes(0x61bf54, 3);
        initials[0] = name[0];
        initials[1] = name[1];
        initials[2] = name[2];
        const auto difficulty = word(0x5e39e0);
        word(0x61bf5c) = rank;
        --rank;
        word(0x61bf50) = 1;
        word(0x61bf58) = selected;
        const auto selected_value = word(0x5f26b4 + selected * 0xecu);
        auto remaining = word(0x5e39dc);
        word(0x61bf60) = selected_value;
        if (std::bit_cast<std::int32_t>(remaining) > 0)
        {
            auto source = 0x5fa724u + (difficulty + selected * 3u) * 0x78u;
            auto destination = 0x61bf6cu;
            do
            {
                // MOV WORD then MOV BYTE, retaining forward overlapping semantics.
                const auto source_initials = f.source.source_bytes(source - 12u, 3);
                const auto low = source_initials[0], high = source_initials[1];
                auto target_initials = f.source.source_bytes(destination + 4u, 3);
                target_initials[0] = low;
                target_initials[1] = high;
                target_initials[2] = source_initials[2];
                auto ecx = word(source), edx = word(source - 4u);
                word(destination) = 0;
                word(destination + 16u) = ecx;
                ecx = word(source + 4u);
                word(destination + 12u) = rank;
                word(destination + 8u) = edx;
                edx = word(source + 8u);
                word(destination + 20u) = ecx;
                word(destination + 24u) = edx;
                --rank;
                --remaining;
                source += 24u;
                destination += 28u;
            } while (remaining != 0);
        }
        calls.prepare_race_43a780(f, 0x61bf50);
        word(0x5e3794) = 4;
        push(0, 25);
        return;
    }
    case 4:
        word(0x5e3794) = 5;
        calls.finish_race_43d500(f, 0);
        return;
    case 5:
        word(0x5e3794) = 2;
        return;
    default:
        return;
    }
}
} // namespace sgn::retail
