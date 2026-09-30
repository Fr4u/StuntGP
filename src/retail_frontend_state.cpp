#include "sgn/retail_frontend_state.hpp"
#include <bit>
#include <type_traits>

namespace sgn::retail
{
namespace
{
template <std::size_t N> std::span<std::uint8_t> bytes(std::array<std::uint32_t, N> &words) noexcept
{
    static_assert(std::is_same_v<std::uint8_t, unsigned char>);
    static_assert(std::endian::native == std::endian::little);
    return {reinterpret_cast<std::uint8_t *>(words.data()), sizeof(words)};
}
bool contains(std::uint32_t base, std::size_t size, std::uint32_t address, std::size_t length)
{
    return address >= base && std::size_t(address - base) <= size && length <= size - std::size_t(address - base);
}
} // namespace
std::span<std::uint8_t, 0x5c> FrontendSourceStorage::page_navigation() noexcept
{
    return std::span<std::uint8_t, 0x5c>{reinterpret_cast<std::uint8_t *>(navigation_words.data()), 0x5c};
}
std::span<const std::uint8_t, 0x5c> FrontendSourceStorage::page_navigation() const noexcept
{
    return std::span<const std::uint8_t, 0x5c>{reinterpret_cast<const std::uint8_t *>(navigation_words.data()), 0x5c};
}
PersistenceView FrontendSourceStorage::persistence_view() noexcept
{
    return {persistence.bytes,
            std::span<std::uint8_t, persistence_body_size>{bytes(cold_suffix).data(), persistence_body_size},
            read_scratch.bytes};
}
FrontendLanguageCatalogViews FrontendSourceStorage::language_catalog_view() noexcept
{
    constexpr auto first = (0x5e37b8 - suffix_base) / 4;
    static_assert(first + frontend_language_catalog_words <= std::tuple_size_v<decltype(cold_suffix)>);
    return {std::span<std::uint32_t, frontend_language_catalog_words>{cold_suffix.data() + first,
                                                                      frontend_language_catalog_words},
            language_text_handles, language_names, language_input_identity};
}
FrontendLanguageStrings FrontendSourceStorage::language_strings() const noexcept
{
    const auto catalog = std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(cold_suffix.data()) +
                                                           (0x5e37b8u - suffix_base),
                                                       frontend_language_catalog_words * 4u};
    const auto empty = std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(&language_empty_sentinel),
                                                     sizeof(language_empty_sentinel)};
    return {language_arena, language_text_handles, language_names, catalog, empty};
}
FrontendLanguageSelectionViews FrontendSourceStorage::language_selection_view() noexcept
{
    return {language_catalog_view(), language_scratch, language_arena, language_descriptors, language_remaining};
}
std::span<std::uint8_t> FrontendSourceStorage::source_bytes(std::uint32_t address, std::size_t size)
{
    if (contains(0x484f38, level_configuration_literals_484f38.size(), address, size))
        return std::span{level_configuration_literals_484f38}.subspan(address - 0x484f38, size);
    if (contains(0x5da264, configuration_string_5da264.size(), address, size))
        return std::span{configuration_string_5da264}.subspan(address - 0x5da264, size);
    if (contains(0x5fd248, sizeof(menu_state), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&menu_state), sizeof(menu_state)}.subspan(address - 0x5fd248,
                                                                                                    size);
    if (contains(0x5fd24c, sizeof(menu_result_5fd24c), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&menu_result_5fd24c), sizeof(menu_result_5fd24c)}.subspan(
            address - 0x5fd24c, size);
    if (contains(0x487c38, sizeof(modify_data_487c38), address, size))
        return bytes(modify_data_487c38).subspan(address - 0x487c38, size);
    if (contains(0x4796b8, sizeof(modify_name_4796b8), address, size))
        return bytes(modify_name_4796b8).subspan(address - 0x4796b8, size);
    if (contains(0x624db8, sizeof(modify_rows_624db8), address, size))
        return bytes(modify_rows_624db8).subspan(address - 0x624db8, size);

    if (contains(0x488df8, sizeof(campaign_score_data_488df8), address, size))
        return bytes(campaign_score_data_488df8).subspan(address - 0x488df8, size);
    if (contains(0x479898, sizeof(campaign_score_name_479898), address, size))
        return bytes(campaign_score_name_479898).subspan(address - 0x479898, size);
    if (contains(0x489168, sizeof(select_team_data_489168), address, size))
        return bytes(select_team_data_489168).subspan(address - 0x489168, size);
    if (contains(0x47975c, sizeof(select_team_name_47975c), address, size))
        return bytes(select_team_name_47975c).subspan(address - 0x47975c, size);
    if (contains(0x486750, sizeof(campaign_menu_data_486750), address, size))
        return bytes(campaign_menu_data_486750).subspan(address - 0x486750, size);
    if (contains(0x624a14, sizeof(campaign_menu_text_624a14), address, size))
        return bytes(campaign_menu_text_624a14).subspan(address - 0x624a14, size);
    if (contains(0x489530, sizeof(select_tree_data_489530), address, size))
        return bytes(select_tree_data_489530).subspan(address - 0x489530, size);
    if (contains(0x627fe0, sizeof(select_tree_state_627fe0), address, size))
        return bytes(select_tree_state_627fe0).subspan(address - 0x627fe0, size);
    if (contains(0x627cd8, sizeof(slot_text_627cd8), address, size))
        return bytes(slot_text_627cd8).subspan(address - 0x627cd8, size);
    if (contains(0x489208, sizeof(text_splash_data_489208), address, size))
        return bytes(text_splash_data_489208).subspan(address - 0x489208, size);
    if (contains(0x61c0a0, sizeof(stunt_launch_61c0a0), address, size))
        return bytes(stunt_launch_61c0a0).subspan(address - 0x61c0a0, size);
    if (contains(0x61c0b8, sizeof(stunt_result_61c0b8), address, size))
        return bytes(stunt_result_61c0b8).subspan(address - 0x61c0b8, size);
    if (contains(0x61c0c0, sizeof(stunt_message_61c0c0), address, size))
        return bytes(stunt_message_61c0c0).subspan(address - 0x61c0c0, size);

    if (contains(0x4883a0, sizeof(configuration_page_data_4883a0), address, size))
        return bytes(configuration_page_data_4883a0).subspan(address - 0x4883a0, size);
    if (contains(0x488610, sizeof(control_page_callbacks_488610), address, size))
        return bytes(control_page_callbacks_488610).subspan(address - 0x488610, size);
    if (contains(0x4881b0, sizeof(enter_name_data_4881b0), address, size))
        return bytes(enter_name_data_4881b0).subspan(address - 0x4881b0, size);
    if (contains(0x489690, sizeof(unlock_page_data_489690), address, size))
        return bytes(unlock_page_data_489690).subspan(address - 0x489690, size);
    if (contains(0x627ff8, sizeof(unlock_messages_627ff8), address, size))
        return bytes(unlock_messages_627ff8).subspan(address - 0x627ff8, size);
    if (contains(0x488d60, sizeof(arcade_score_data_488d60), address, size))
        return bytes(arcade_score_data_488d60).subspan(address - 0x488d60, size);
    if (contains(0x627c94, sizeof(arcade_score_text_627c94), address, size))
        return bytes(arcade_score_text_627c94).subspan(address - 0x627c94, size);
    if (contains(0x488920, sizeof(records_page_data_488920), address, size))
        return bytes(records_page_data_488920).subspan(address - 0x488920, size);
    if (contains(0x488cc0, sizeof(high_score_data_488cc0), address, size))
        return bytes(high_score_data_488cc0).subspan(address - 0x488cc0, size);
    if (contains(0x479664, sizeof(records_literals_479664), address, size))
        return bytes(records_literals_479664).subspan(address - 0x479664, size);
    if (contains(0x6250f8, sizeof(quick_score_text_6250f8), address, size))
        return bytes(quick_score_text_6250f8).subspan(address - 0x6250f8, size);
    if (contains(0x625138, sizeof(record_track_rows_625138), address, size))
        return bytes(record_track_rows_625138).subspan(address - 0x625138, size);
    if (contains(0x627c54, sizeof(record_text_627c54), address, size))
        return bytes(record_text_627c54).subspan(address - 0x627c54, size);
    if (contains(0x488c00, sizeof(save_page_data_488c00), address, size))
        return bytes(save_page_data_488c00).subspan(address - 0x488c00, size);
    if (contains(0x627a54, sizeof(save_scratch_627a54), address, size))
        return bytes(save_scratch_627a54).subspan(address - 0x627a54, size);
    if (contains(0x5df414, background_text_5df414.size(), address, size))
        return std::span{background_text_5df414}.subspan(address - 0x5df414, size);
    const auto audio_options = bytes(audio_options_data_4884e0);
    if (contains(0x4884e0, audio_options.size(), address, size))
        return audio_options.subspan(address - 0x4884e0, size);
    const auto video_data = bytes(video_options_data_488588);
    if (contains(0x488588, video_data.size(), address, size))
        return video_data.subspan(address - 0x488588, size);
    const auto options_data = bytes(options_page_data_4881f8);
    if (contains(0x4881f8, options_data.size(), address, size))
        return options_data.subspan(address - 0x4881f8, size);
    const auto host_menu = bytes(host_menu_r23); // NEW HOST (R23) GRAPHICS menu words, not a PE owner
    if (contains(host_menu_base_r23, host_menu.size(), address, size))
        return host_menu.subspan(address - host_menu_base_r23, size);
    if (contains(0x4868d8, sizeof(car_page_data_4868d8), address, size))
        return bytes(car_page_data_4868d8).subspan(address - 0x4868d8, size);
    if (contains(0x624b18, sizeof(car_page_rows_624b18), address, size))
        return bytes(car_page_rows_624b18).subspan(address - 0x624b18, size);
    if (contains(0x488f90, sizeof(slot_bmp_page_data_488f90), address, size))
        return bytes(slot_bmp_page_data_488f90).subspan(address - 0x488f90, size);
    if (contains(0x4892a0, sizeof(track_page_data_4892a0), address, size))
        return bytes(track_page_data_4892a0).subspan(address - 0x4892a0, size);
    if (contains(0x487934, sizeof(credits_page_data_487934), address, size))
        return bytes(credits_page_data_487934).subspan(address - 0x487934, size);
    if (contains(0x4879c0, sizeof(start_page_callbacks_4879c0), address, size))
        return bytes(start_page_callbacks_4879c0).subspan(address - 0x4879c0, size);
    if (contains(0x4879c8, sizeof(language_page_data_4879c8), address, size))
        return bytes(language_page_data_4879c8).subspan(address - 0x4879c8, size);
    if (contains(0x624bb8, sizeof(language_page_rows_624bb8), address, size))
        return bytes(language_page_rows_624bb8).subspan(address - 0x624bb8, size);
    if (contains(0x488e80, sizeof(setup_page_callbacks_488e80), address, size))
        return bytes(setup_page_callbacks_488e80).subspan(address - 0x488e80, size);
    if (contains(0x4869e8, sizeof(catalog_page_data_4869e8), address, size))
        return bytes(catalog_page_data_4869e8).subspan(address - 0x4869e8, size);
    if (contains(0x488108, sizeof(multi_page_data_488108), address, size))
        return bytes(multi_page_data_488108).subspan(address - 0x488108, size);
    const auto single_data = bytes(single_page_data_488e88), game_callbacks = bytes(game_loop_callbacks_487a50);
    if (contains(0x488e88, single_data.size(), address, size))
        return single_data.subspan(address - 0x488e88, size);
    if (contains(0x487a50, game_callbacks.size(), address, size))
        return game_callbacks.subspan(address - 0x487a50, size);
    const auto main_data = bytes(main_page_data_487a58), text_callbacks = bytes(menu_text_callbacks_4794e8);
    if (contains(0x487a58, main_data.size(), address, size))
        return main_data.subspan(address - 0x487a58, size);
    if (contains(0x4794e8, text_callbacks.size(), address, size))
        return text_callbacks.subspan(address - 0x4794e8, size);
    const auto words_5da2e8 = bytes(menu_words_5da2e8);
    if (contains(0x5da2e8, words_5da2e8.size(), address, size))
        return words_5da2e8.subspan(address - 0x5da2e8, size);
    const auto clip = bytes(frontend_clip);
    if (contains(0x5da360, clip.size(), address, size))
        return clip.subspan(address - 0x5da360, size);
    if (contains(0x5da370, sizeof(menu_counter_5da370), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&menu_counter_5da370), sizeof(menu_counter_5da370)}.subspan(
            address - 0x5da370, size);
    const auto menu_payload = bytes(menu_payload_5da374);
    if (contains(0x5da374, menu_payload.size(), address, size))
        return menu_payload.subspan(address - 0x5da374, size);
    if (contains(0x5df514, sizeof(menu_peak_5df514), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&menu_peak_5df514), sizeof(menu_peak_5df514)}.subspan(
            address - 0x5df514, size);
    const auto counts = bytes(menu_draw_counts_5df518);
    if (contains(0x5df518, counts.size(), address, size))
        return counts.subspan(address - 0x5df518, size);
    if (contains(0x4793d4, sizeof(menu_last_material_4793d4), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&menu_last_material_4793d4),
                         sizeof(menu_last_material_4793d4)}
            .subspan(address - 0x4793d4, size);
    const auto menu_words = bytes(menu_words_5df194);
    if (contains(0x5df194, menu_words.size(), address, size))
        return menu_words.subspan(address - 0x5df194, size);
    if (contains(cold_base, persistence_size, address, size))
        return std::span{persistence.bytes}.subspan(address - cold_base, size);
    const auto suffix = bytes(cold_suffix);
    if (contains(suffix_base, suffix.size(), address, size))
        return suffix.subspan(address - suffix_base, size);
    if (contains(0x625938, persistence_size, address, size))
        return std::span{read_scratch.bytes}.subspan(address - 0x625938, size);
    if (contains(0x5fd048, csv_float_scratch.size(), address, size))
        return std::span{csv_float_scratch}.subspan(address - 0x5fd048, size);
    if (contains(0x5fd148, csv_token_scratch.size(), address, size))
        return std::span{csv_token_scratch}.subspan(address - 0x5fd148, size);
    if (contains(0x47966c, initials_scratch.size(), address, size))
        return std::span{initials_scratch}.subspan(address - 0x47966c, size);
    if (contains(0x5ff3a8, selection_time_scratch.size(), address, size))
        return std::span{selection_time_scratch}.subspan(address - 0x5ff3a8, size);
    if (contains(0x5ff328, numeric_scratch.size(), address, size))
        return std::span{numeric_scratch}.subspan(address - 0x5ff328, size);
    if (contains(0x5fd258, page_pool.size(), address, size))
        return std::span{page_pool}.subspan(address - 0x5fd258, size);
    const auto navigation = bytes(navigation_words);
    if (contains(0x5ff3e8, navigation.size(), address, size))
        return navigation.subspan(address - 0x5ff3e8, size);
    if (contains(0x5ff4b8, sizeof(packed_word_5ff4b8), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&packed_word_5ff4b8), sizeof(packed_word_5ff4b8)}.subspan(
            address - 0x5ff4b8, size);
    if (contains(0x61b9c0, sizeof(challenge_result_61b9c0), address, size))
        return bytes(challenge_result_61b9c0).subspan(address - 0x61b9c0, size);
    if (contains(0x61b9c8, challenge_text.size(), address, size))
        return std::span{challenge_text}.subspan(address - 0x61b9c8, size);
    if (contains(0x61bdcc, campaign_text.size(), address, size))
        return std::span{campaign_text}.subspan(address - 0x61bdcc, size);
    if (contains(0x5ff8c0, sizeof(feedback_gate), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&feedback_gate), sizeof(feedback_gate)}.subspan(
            address - 0x5ff8c0, size);
    if (contains(0x5ff8c4, language_scratch.size(), address, size))
        return std::span{language_scratch}.subspan(address - 0x5ff8c4, size);
    if (contains(0x5ffcc8, language_arena.size(), address, size))
        return std::span{language_arena}.subspan(address - 0x5ffcc8, size);
    const auto text_handles = bytes(language_text_handles), names = bytes(language_names);
    if (contains(0x605cc8, text_handles.size(), address, size))
        return text_handles.subspan(address - 0x605cc8, size);
    if (contains(0x606838, names.size(), address, size))
        return names.subspan(address - 0x606838, size);
    if (contains(0x6087a0, sizeof(language_input_identity), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&language_input_identity), sizeof(language_input_identity)}
            .subspan(address - 0x6087a0, size);
    const auto descriptors = bytes(language_descriptors);
    if (contains(0x606860, descriptors.size(), address, size))
        return descriptors.subspan(address - 0x606860, size);
    if (contains(0x47b1a0, sizeof(language_remaining), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&language_remaining), sizeof(language_remaining)}.subspan(
            address - 0x47b1a0, size);
    if (contains(0x6087a4, sizeof(language_empty_sentinel), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&language_empty_sentinel), sizeof(language_empty_sentinel)}
            .subspan(address - 0x6087a4, size);
    const auto frontend_records = bytes(frontend_descriptors);
    if (contains(0x608d88, frontend_records.size(), address, size))
        return frontend_records.subspan(address - 0x608d88, size);
    if (contains(0x61c1c0, sizeof(time_trial_progress_61c1c0), address, size))
        return bytes(time_trial_progress_61c1c0).subspan(address - 0x61c1c0, size);
    if (contains(0x61c1d0, sizeof(time_trial_launch_61c1d0), address, size))
        return bytes(time_trial_launch_61c1d0).subspan(address - 0x61c1d0, size);
    if (contains(0x61c1f0, sizeof(time_trial_result_61c1f0), address, size))
        return bytes(time_trial_result_61c1f0).subspan(address - 0x61c1f0, size);
    if (contains(0x61c204, sizeof(time_trial_code_state_61c204), address, size))
        return std::span{reinterpret_cast<std::uint8_t *>(&time_trial_code_state_61c204),
                         sizeof(time_trial_code_state_61c204)}
            .subspan(address - 0x61c204, size);
    if (contains(0x48642c, sizeof(time_trial_codes_48642c), address, size))
        return bytes(time_trial_codes_48642c).subspan(address - 0x48642c, size);
    if (contains(0x61b880, sizeof(championship_launch_61b880), address, size))
        return bytes(championship_launch_61b880).subspan(address - 0x61b880, size);
    if (contains(0x61b940, sizeof(championship_result_61b940), address, size))
        return bytes(championship_result_61b940).subspan(address - 0x61b940, size);
    const auto launch = bytes(campaign_launch), result = bytes(campaign_result);
    const auto single_launch = bytes(single_race_launch);
    if (contains(0x61bf50, single_launch.size(), address, size))
        return single_launch.subspan(address - 0x61bf50, size);
    if (contains(0x61bbe0, launch.size(), address, size))
        return launch.subspan(address - 0x61bbe0, size);
    if (contains(0x61bed0, result.size(), address, size))
        return result.subspan(address - 0x61bed0, size);
    throw FormatError("frontend source view crosses an owner or names an unowned region");
}
std::uint32_t &FrontendSourceStorage::source_word(std::uint32_t address)
{
    if (address == 0x5fd248)
        return menu_state;
    if (address == 0x5fd24c)
        return menu_result_5fd24c;
    if ((address & 3u) != 0)
        throw FormatError("frontend source word is not aligned");
    if (contains(0x487c38, sizeof(modify_data_487c38), address, 4))
        return modify_data_487c38[(address - 0x487c38) / 4];
    if (contains(0x4796b8, sizeof(modify_name_4796b8), address, 4))
        return modify_name_4796b8[(address - 0x4796b8) / 4];
    if (contains(0x624db8, sizeof(modify_rows_624db8), address, 4))
        return modify_rows_624db8[(address - 0x624db8) / 4];

    if (contains(0x488df8, sizeof(campaign_score_data_488df8), address, 4))
        return campaign_score_data_488df8[(address - 0x488df8) / 4];
    if (contains(0x479898, sizeof(campaign_score_name_479898), address, 4))
        return campaign_score_name_479898[(address - 0x479898) / 4];
    if (contains(0x489168, sizeof(select_team_data_489168), address, 4))
        return select_team_data_489168[(address - 0x489168) / 4];
    if (contains(0x47975c, sizeof(select_team_name_47975c), address, 4))
        return select_team_name_47975c[(address - 0x47975c) / 4];
    if (contains(0x486750, sizeof(campaign_menu_data_486750), address, 4))
        return campaign_menu_data_486750[(address - 0x486750) / 4];
    if (contains(0x624a14, sizeof(campaign_menu_text_624a14), address, 4))
        return campaign_menu_text_624a14[(address - 0x624a14) / 4];
    if (contains(0x489530, sizeof(select_tree_data_489530), address, 4))
        return select_tree_data_489530[(address - 0x489530) / 4];
    if (contains(0x627fe0, sizeof(select_tree_state_627fe0), address, 4))
        return select_tree_state_627fe0[(address - 0x627fe0) / 4];
    if (contains(0x627cd8, sizeof(slot_text_627cd8), address, 4))
        return slot_text_627cd8[(address - 0x627cd8) / 4];
    if (contains(0x61b9c0, sizeof(challenge_result_61b9c0), address, 4))
        return challenge_result_61b9c0[(address - 0x61b9c0) / 4];
    if (contains(0x489208, sizeof(text_splash_data_489208), address, 4))
        return text_splash_data_489208[(address - 0x489208) / 4];
    if (contains(0x61c0a0, sizeof(stunt_launch_61c0a0), address, 4))
        return stunt_launch_61c0a0[(address - 0x61c0a0) / 4];
    if (contains(0x61c0b8, sizeof(stunt_result_61c0b8), address, 4))
        return stunt_result_61c0b8[(address - 0x61c0b8) / 4];
    if (contains(0x61c0c0, sizeof(stunt_message_61c0c0), address, 4))
        return stunt_message_61c0c0[(address - 0x61c0c0) / 4];

    if (contains(0x4883a0, sizeof(configuration_page_data_4883a0), address, 4))
        return configuration_page_data_4883a0[(address - 0x4883a0) / 4];
    if (contains(0x488610, sizeof(control_page_callbacks_488610), address, 4))
        return control_page_callbacks_488610[(address - 0x488610) / 4];
    if (contains(0x4881b0, sizeof(enter_name_data_4881b0), address, 4))
        return enter_name_data_4881b0[(address - 0x4881b0) / 4];
    if (contains(0x489690, sizeof(unlock_page_data_489690), address, 4))
        return unlock_page_data_489690[(address - 0x489690) / 4];
    if (contains(0x627ff8, sizeof(unlock_messages_627ff8), address, 4))
        return unlock_messages_627ff8[(address - 0x627ff8) / 4];
    if (contains(0x488d60, sizeof(arcade_score_data_488d60), address, 4))
        return arcade_score_data_488d60[(address - 0x488d60) / 4];
    if (contains(0x627c94, sizeof(arcade_score_text_627c94), address, 4))
        return arcade_score_text_627c94[(address - 0x627c94) / 4];
    if (contains(0x488920, sizeof(records_page_data_488920), address, 4))
        return records_page_data_488920[(address - 0x488920) / 4];
    if (contains(0x488cc0, sizeof(high_score_data_488cc0), address, 4))
        return high_score_data_488cc0[(address - 0x488cc0) / 4];
    if (contains(0x479664, sizeof(records_literals_479664), address, 4))
        return records_literals_479664[(address - 0x479664) / 4];
    if (contains(0x6250f8, sizeof(quick_score_text_6250f8), address, 4))
        return quick_score_text_6250f8[(address - 0x6250f8) / 4];
    if (contains(0x625138, sizeof(record_track_rows_625138), address, 4))
        return record_track_rows_625138[(address - 0x625138) / 4];
    if (contains(0x627c54, sizeof(record_text_627c54), address, 4))
        return record_text_627c54[(address - 0x627c54) / 4];
    if (contains(0x488c00, sizeof(save_page_data_488c00), address, 4))
        return save_page_data_488c00[(address - 0x488c00) / 4];
    if (contains(0x627a54, sizeof(save_scratch_627a54), address, 4))
        return save_scratch_627a54[(address - 0x627a54) / 4];
    if (contains(0x4884e0, sizeof(audio_options_data_4884e0), address, 4))
        return audio_options_data_4884e0[(address - 0x4884e0) / 4];
    if (contains(0x488588, sizeof(video_options_data_488588), address, 4))
        return video_options_data_488588[(address - 0x488588) / 4];
    if (contains(0x4881f8, sizeof(options_page_data_4881f8), address, 4))
        return options_page_data_4881f8[(address - 0x4881f8) / 4];
    if (contains(host_menu_base_r23, sizeof(host_menu_r23), address, 4))
        return host_menu_r23[(address - host_menu_base_r23) / 4]; // NEW HOST (R23)
    if (contains(0x4868d8, sizeof(car_page_data_4868d8), address, 4))
        return car_page_data_4868d8[(address - 0x4868d8) / 4];
    if (contains(0x624b18, sizeof(car_page_rows_624b18), address, 4))
        return car_page_rows_624b18[(address - 0x624b18) / 4];
    if (contains(0x488f90, sizeof(slot_bmp_page_data_488f90), address, 4))
        return slot_bmp_page_data_488f90[(address - 0x488f90) / 4];
    if (contains(0x4892a0, sizeof(track_page_data_4892a0), address, 4))
        return track_page_data_4892a0[(address - 0x4892a0) / 4];
    if (contains(0x487934, sizeof(credits_page_data_487934), address, 4))
        return credits_page_data_487934[(address - 0x487934) / 4];
    if (contains(0x4879c0, sizeof(start_page_callbacks_4879c0), address, 4))
        return start_page_callbacks_4879c0[(address - 0x4879c0) / 4];
    if (contains(0x4879c8, sizeof(language_page_data_4879c8), address, 4))
        return language_page_data_4879c8[(address - 0x4879c8) / 4];
    if (contains(0x624bb8, sizeof(language_page_rows_624bb8), address, 4))
        return language_page_rows_624bb8[(address - 0x624bb8) / 4];
    if (contains(0x4869e8, sizeof(catalog_page_data_4869e8), address, 4))
        return catalog_page_data_4869e8[(address - 0x4869e8) / 4];
    if (contains(0x488108, sizeof(multi_page_data_488108), address, 4))
        return multi_page_data_488108[(address - 0x488108) / 4];
    if (contains(0x488e80, sizeof(setup_page_callbacks_488e80), address, 4))
        return setup_page_callbacks_488e80[(address - 0x488e80) / 4];
    if (contains(0x488e88, sizeof(single_page_data_488e88), address, 4))
        return single_page_data_488e88[(address - 0x488e88) / 4];
    if (contains(0x487a50, sizeof(game_loop_callbacks_487a50), address, 4))
        return game_loop_callbacks_487a50[(address - 0x487a50) / 4];
    if (contains(0x487a58, sizeof(main_page_data_487a58), address, 4))
        return main_page_data_487a58[(address - 0x487a58) / 4];
    if (contains(0x4794e8, sizeof(menu_text_callbacks_4794e8), address, 4))
        return menu_text_callbacks_4794e8[(address - 0x4794e8) / 4];
    if (contains(0x5da2e8, sizeof(menu_words_5da2e8), address, 4))
        return menu_words_5da2e8[(address - 0x5da2e8) / 4];
    if (contains(0x5da360, sizeof(frontend_clip), address, 4))
        return frontend_clip[(address - 0x5da360) / 4];
    if (address == 0x5da370)
        return menu_counter_5da370;
    if (contains(0x5da374, sizeof(menu_payload_5da374), address, 4))
        return menu_payload_5da374[(address - 0x5da374) / 4];
    if (address == 0x5df514)
        return menu_peak_5df514;
    if (contains(0x5df518, sizeof(menu_draw_counts_5df518), address, 4))
        return menu_draw_counts_5df518[(address - 0x5df518) / 4];
    if (address == 0x4793d4)
        return menu_last_material_4793d4;
    if (contains(0x5df194, sizeof(menu_words_5df194), address, 4))
        return menu_words_5df194[(address - 0x5df194) / 4];
    if (contains(0x608d88, sizeof(frontend_descriptors), address, 4))
        return frontend_descriptors[(address - 0x608d88) / 4];
    if (address == 0x5ff8c0)
        return feedback_gate;
    if (contains(0x5ff3e8, sizeof(navigation_words), address, 4))
        return navigation_words[(address - 0x5ff3e8) / 4];
    if (address == 0x5ff4b8)
        return packed_word_5ff4b8;
    if (address == 0x6087a0)
        return language_input_identity;
    if (address == 0x6087a4)
        return language_empty_sentinel;
    if (address == 0x47b1a0)
        return language_remaining;
    if (contains(0x606860, sizeof(language_descriptors), address, 4))
        return language_descriptors[(address - 0x606860) / 4];
    if (contains(0x605cc8, sizeof(language_text_handles), address, 4))
        return language_text_handles[(address - 0x605cc8) / 4];
    if (contains(0x606838, sizeof(language_names), address, 4))
        return language_names[(address - 0x606838) / 4];
    if (contains(suffix_base, sizeof(cold_suffix), address, 4))
        return cold_suffix[(address - suffix_base) / 4];
    if (contains(0x61c1c0, sizeof(time_trial_progress_61c1c0), address, 4))
        return time_trial_progress_61c1c0[(address - 0x61c1c0) / 4];
    if (contains(0x61c1d0, sizeof(time_trial_launch_61c1d0), address, 4))
        return time_trial_launch_61c1d0[(address - 0x61c1d0) / 4];
    if (contains(0x61c1f0, sizeof(time_trial_result_61c1f0), address, 4))
        return time_trial_result_61c1f0[(address - 0x61c1f0) / 4];
    if (address == 0x61c204)
        return time_trial_code_state_61c204;
    if (contains(0x48642c, sizeof(time_trial_codes_48642c), address, 4))
        return time_trial_codes_48642c[(address - 0x48642c) / 4];
    if (contains(0x61b880, sizeof(championship_launch_61b880), address, 4))
        return championship_launch_61b880[(address - 0x61b880) / 4];
    if (contains(0x61b940, sizeof(championship_result_61b940), address, 4))
        return championship_result_61b940[(address - 0x61b940) / 4];
    if (contains(0x61bbe0, sizeof(campaign_launch), address, 4))
        return campaign_launch[(address - 0x61bbe0) / 4];
    if (contains(0x61bf50, sizeof(single_race_launch), address, 4))
        return single_race_launch[(address - 0x61bf50) / 4];
    if (contains(0x61bed0, sizeof(campaign_result), address, 4))
        return campaign_result[(address - 0x61bed0) / 4];
    throw FormatError("frontend word reference requires its actual typed owner");
}
void FrontendSourceStorage::bind_campaign_return()
{
    race_return_bindings[5] = {bytes(campaign_launch), bytes(campaign_result)};
}
void FrontendSourceStorage::bind_time_trial_return()
{
    race_return_bindings[3].roster = bytes(time_trial_launch_61c1d0); // 5DF534
    race_return_bindings[3].result = bytes(time_trial_result_61c1f0); // 5DF538
}
RaceSessionState &FrontendState::world() noexcept
{
    return *world_;
}
const RaceSessionState &FrontendState::world() const noexcept
{
    return *world_;
}
void clear_frontend_cold_storage(FrontendSourceStorage &state) noexcept
{
    state.persistence.bytes.fill(0);
    state.cold_suffix.fill(0);
}
void initialize_frontend_lookup_tables(FrontendSourceStorage &state) noexcept
{
    state.lookup_a.fill(0xffffffffu);
    state.lookup_b.fill(0xffffffffu);
    state.lookup_c.fill(0x10000u);
}
void return_to_frontend(FrontendSourceStorage &state) noexcept
{
    // 43D1F0 is an actual source no-op thunk. No synthetic service call needed.
    state.menu_state = 0;
    initialize_frontend_lookup_tables(state);
}
} // namespace sgn::retail
