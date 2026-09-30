#include "sgn/source_frontend_csv_lower.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <array>
#include <algorithm>
#include <string_view>
#include <stdexcept>
namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
std::string_view label_text(std::span<const std::uint8_t> bytes)
{
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
    if (end == bytes.end())
        throw std::runtime_error("CSV label string leaves its represented owner");
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<std::size_t>(end - bytes.begin())};
}
struct MusicField
{
    Word key;
    Word destination;
};
constexpr std::array<MusicField, 49> music_fields{{
    {0x478cd4u, 0x5fcab4u}, // feMusic_Title
    {0x478ce4u, 0x5fcab8u}, // feMusic_MenuMain
    {0x478cf8u, 0x5fcabcu}, // feMusic_MenuSingle
    {0x478d0cu, 0x5fcac0u}, // feMusic_MenuMulti
    {0x478d20u, 0x5fcac4u}, // feMusic_MenuCatalog
    {0x478d34u, 0x5fcac8u}, // feMusic_MenuRecords
    {0x478d48u, 0x5fcaccu}, // feMusic_MenuOptions
    {0x478d5cu, 0x5fcad0u}, // feMusic_ArcadeName
    {0x478d70u, 0x5fcad4u}, // feMusic_ArcadeCar
    {0x478d84u, 0x5fcad8u}, // feMusic_ArcadePreview
    {0x478d9cu, 0x5fcadcu}, // feMusic_ArcadeAwards
    {0x478db4u, 0x5fcae0u}, // feMusic_ArcadeUnlock
    {0x478dccu, 0x5fcae4u}, // feMusic_ArcadeRoute
    {0x478de0u, 0x5fcae8u}, // feMusic_ArcadeComplete
    {0x478df8u, 0x5fcaecu}, // feMusic_ArcadeFailed
    {0x478e10u, 0x5fcaf0u}, // feMusic_ArcadeScores
    {0x478e28u, 0x5fcaf4u}, // feMusic_CampaignSlot
    {0x478e40u, 0x5fcaf8u}, // feMusic_CampaignName
    {0x478e58u, 0x5fcafcu}, // feMusic_CampaignTeam
    {0x478e70u, 0x5fcb00u}, // feMusic_CampaignCar
    {0x478e84u, 0x5fcb04u}, // feMusic_CampaignMenu
    {0x478e9cu, 0x5fcb08u}, // feMusic_CampaignModify
    {0x478eb4u, 0x5fcb0cu}, // feMusic_CampaignPreview
    {0x478eccu, 0x5fcb10u}, // feMusic_QuickPads
    {0x478ee0u, 0x5fcb14u}, // feMusic_QuickCar
    {0x478ef4u, 0x5fcb18u}, // feMusic_QuickTrack
    {0x478f08u, 0x5fcb1cu}, // feMusic_QuickScores
    {0x478f1cu, 0x5fcb20u}, // feMusic_TournamentOptions
    {0x478f38u, 0x5fcb24u}, // feMusic_TournamentPads
    {0x478f50u, 0x5fcb28u}, // feMusic_TournamentCar
    {0x478f68u, 0x5fcb2cu}, // feMusic_TournamentTrack
    {0x478f80u, 0x5fcb30u}, // feMusic_TournamentScores
    {0x478f9cu, 0x5fcb34u}, // feMusic_PracticeCar
    {0x478fb0u, 0x5fcb38u}, // feMusic_PracticeTrack
    {0x478fc8u, 0x5fcb3cu}, // feMusic_StuntName
    {0x478fdcu, 0x5fcb40u}, // feMusic_StuntInfo
    {0x478ff0u, 0x5fcb44u}, // feMusic_StuntScores
    {0x479004u, 0x5fcb48u}, // feMusic_StuntUnlock
    {0x479018u, 0x5fcb4cu}, // feMusic_TrialName
    {0x47902cu, 0x5fcb50u}, // feMusic_TrialCar
    {0x479040u, 0x5fcb54u}, // feMusic_TrialTrack
    {0x479054u, 0x5fcb58u}, // feMusic_TrialScores
    {0x479068u, 0x5fcb5cu}, // feMusic_TrialUnlock
    {0x47907cu, 0x5fcb60u}, // feMusic_CampaignPostRace
    {0x479098u, 0x5fcb64u}, // feMusic_CampaignPreBonus
    {0x4790b4u, 0x5fcb68u}, // feMusic_CampaignPostBonus
    {0x4790d0u, 0x5fcb6cu}, // feMusic_CampaignPreFinal
    {0x4790ecu, 0x5fcb70u}, // feMusic_CampaignUnlock
    {0x479104u, 0x5fcb74u}, // feMusic_CampaignScores
}};
} // namespace
std::uint32_t parse_frontend_csv_435010(SourceFrontendCsv &csv)
{
    auto &f = csv.frontend;
    for (Word at = 0x5fcab0; at < 0x5fcb78; at += 4)
        f.source.source_word(at) = 0; // 435023 REP50DWORD
    csv.next_line_43c5c0();
    for (const auto &field : music_fields)
    {
        (void)csv.label_43c600(label_text(csv.literal(field.key)));
        (void)csv.number_43c770(f.source.source_word(field.destination));
        csv.next_line_43c5c0();
    }
    return 1;
}
std::uint32_t parse_frontend_csv_4356f0(SourceFrontendCsv &csv)
{
    (void)csv.label_43c600(label_text(csv.literal(0x47911c)));
    (void)csv.number_43c770(csv.frontend.source.source_word(0x5fcaac));
    csv.next_line_43c5c0();
    return 1;
}
std::uint32_t parse_frontend_csv_434f60(SourceFrontendCsv &csv)
{
    csv.next_line_43c5c0();
    std::array<std::uint8_t, 64> label;
    for (Word index = 1, destination = 0x5fcb90; destination < 0x5fcde8; ++index, destination += 0x18)
    {
        const std::array<Word, 1> arguments{index};
        (void)csv.format_key(0x478cd0, arguments, label, 0x434f90);
        (void)csv.label_43c600(label_text(label));
        for (Word field = 0; field < 6; ++field)
            (void)csv.number_43c770(csv.frontend.source.source_word(destination + field * 4u));
        csv.next_line_43c5c0();
    }
    return 1;
}
std::uint32_t parse_frontend_csv_435730(SourceFrontendCsv &csv)
{
    auto &f = csv.frontend;
    (void)csv.label_43c600(label_text(csv.literal(0x479130)));
    (void)csv.number_43c770(f.source.source_word(0x5fcaa8));
    csv.next_line_43c5c0();
    std::array<std::uint8_t, 64> label;
    for (Word race = 1, destination = 0x5fc608; destination < 0x5fcaa8; ++race, destination += 0x94)
    {
        const std::array<Word, 1> arguments{race};
        (void)csv.format_key(0x479140, arguments, label, 0x43577a);
        (void)csv.label_43c600(label_text(label));
        (void)csv.number_43c770(f.source.source_word(destination));
        csv.next_line_43c5c0();
    }
    csv.next_line_43c5c0(); // 4357B0, distinct additional source call after the eighth track row
    for (Word race = 1, record = 0x5fc60c; record < 0x5fcaac; ++race, record += 0x94)
    {
        for (Word car = 1, entry = record; car <= 6; ++car, entry += 0x18)
        {
            const std::array<Word, 2> arguments{race, car};
            (void)csv.format_key(0x47914c, arguments, label, 0x4357de);
            (void)csv.label_43c600(label_text(label));
            constexpr Word cold_end =
                FrontendSourceStorage::cold_base + static_cast<Word>(FrontendSourceStorage::cold_bytes);
            // 43C680 source destination has no caller-provided length. Bound only
            // at the represented owner; do not invent an8-byte name truncation.
            (void)csv.token_43c680(f.source.source_bytes(entry, cold_end - entry));
            for (Word field = 0; field < 4; ++field)
                (void)csv.number_43c770(f.source.source_word(entry + 8u + field * 4u));
            csv.next_line_43c5c0();
        }
    }
    return 1;
}
} // namespace sgn::retail
