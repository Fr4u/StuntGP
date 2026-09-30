#include "sgn/source_main_menu_children.hpp"
#include "sgn/source_championship_mode.hpp"
#include "sgn/source_campaign_mode.hpp"
#include "sgn/source_campaign_slot_page.hpp"
#include "sgn/source_arcade_score_page.hpp"
#include "sgn/source_select_tree_page.hpp"
#include "sgn/source_select_team_page.hpp"
#include "sgn/source_campaign_menu_page.hpp"
#include "sgn/source_modify_page.hpp"
#include "sgn/source_campaign_score_page.hpp"
#include "sgn/source_stunt_challenge_mode.hpp"
#include "sgn/source_text_splash_page.hpp"
#include "sgn/source_enter_name_page.hpp"
#include "sgn/source_time_trial_mode.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/source_demo_race.hpp"
#include "sgn/source_save_page_draw.hpp"
#include "sgn/source_audio_options_page.hpp"
#include "sgn/source_save_page.hpp"
#include "sgn/source_setup_page.hpp"
#include "sgn/source_multi_page.hpp"
#include "sgn/source_catalog_page.hpp"
#include "sgn/source_records_pages.hpp"
#include "sgn/source_unlock_page.hpp"
#include "sgn/source_credits_page.hpp"
#include "sgn/source_bmp_splash_page.hpp"
#include "sgn/source_control_configuration_pages.hpp"
#include "sgn/source_language_page.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/source_selection_pages.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include "sgn/source_menu_audio_maintenance.hpp"
#include "sgn/source_menu_camera_geometry.hpp"
#include "sgn/race_session.hpp"
#include <type_traits>

namespace sgn::retail
{
using Word = std::uint32_t;
// NEW application wiring of existing recovered children. No state is kept
// here: the supplied frontend resolves the same App and replay buffer owners.
Word &SourceMainMenuRequiredServices::demo_index_627fdc(FrontendState &f)
{
    return source_demo_index_627fdc(f);
}
void SourceMainMenuRequiredServices::load_demo_43b0e0(FrontendState &f, Word record)
{
    prepare_frontend_demo_43b0e0(f, record);
}
Word SourceMainMenuRequiredServices::replay_time_word_45811b(FrontendState &f, Word identity)
{
    return read_source_replay_time_45811b(f, identity);
}
SourceMainMenuChildren::SourceMainMenuChildren(FrontendState &frontend, NativeSourceMenuCrt &crt,
                                               SourceApplicationShutdownServices &shutdown,
                                               MenuKeyboardAcquisition &keys, Win32MenuDirectInputStartup &input,
                                               CampaignScoreMusic &music, SessionOuterFrameServices &frames,
                                               PolygonSubmissionSink &sink, SourceFrontendMaterialBinding &materials,
                                               std::uint8_t &character, FrontendPerformanceCounter &counter,
                                               SourceMenuSampleNative &samples, SourceSavePageServices &save,
                                               SourceMainMenuRequiredServices &required)
    : SourceMenuLanguageChildren(keys, input, music, frames, sink, materials, character), frontend_(frontend),
      crt_(crt), shutdown_(shutdown), music_(music), counter_(counter), samples_(samples), save_(save),
      required_(required)
{
    // Core validates before publishing its material borrow; this also protects
    // direct construction of this inert receiver against another CRT owner.
    crt_.require_platform(*frontend_.shared_platform());
}

Word SourceMainMenuChildren::create_other_page(FrontendState &f, Word key, Word kind)
{
    if (kind == 16)
        return create_frontend_configuration_page_43e000(f, key, *this, *this);
    if (kind == 19)
        return create_frontend_control_page_43e000(f, key);
    if (kind == 28)
        return create_frontend_campaign_slot_page_43e000(f, key, *this, *this);
    if (kind == 31)
        return create_frontend_arcade_score_page_43e000(f, key, *this, *this);
    if (kind == 12)
        return create_frontend_select_tree_page_43e000(f, key, *this, *this);
    if (kind == 13)
        return create_frontend_select_team_page_43e000(f, key, *this, *this);
    if (kind == 14)
        return create_frontend_campaign_menu_page_43e000(f, key, *this, *this);
    if (kind == 7)
        return create_frontend_modify_page_43e000(f, key, *this, *this);
    if (kind == 32)
        return create_frontend_campaign_score_page_43e000(f, key, *this, *this);
    if (kind == 38)
        return create_frontend_text_splash_page_43e000(f, key, *this, *this);
    if (kind == 37)
        return create_frontend_bmp_splash_page_43e000(f, key, *this);
    if (kind == 30)
        return create_frontend_credits_page_43e000(f, key, *this);
    if (kind == 9)
        return create_frontend_enter_name_page_43e000(f, key);
    if (kind == 5)
        return create_frontend_setup_page_43e000(f, key);
    if (kind == 6)
        return create_frontend_multi_page_43e000(f, key, *this, *this);
    if (kind == 8)
        return create_frontend_catalog_page_43e000(f, key, *this, *this);
    if (kind == 21 || kind == 34 || kind == 35 || kind == 39)
        return create_frontend_records_page_43e000(f, key, kind, *this, *this);
    if (kind == 40)
        return create_frontend_unlock_page_43e000(f, key, *this, *this);
    if (kind == 29)
        return create_frontend_language_page_43e000(f, key, *this, *this, *this);
    if (kind == 17)
        return create_frontend_audio_options_43e000(f, key, *this, *this);
    if (kind == 27)
        return create_frontend_save_page_43e000(f, key, *this, *this, save_);
    return required_.create_remaining_page(f, key, kind);
}
void SourceMainMenuChildren::close_other_page(FrontendState &f, Word page, Word callback)
{
    if (callback == 0x455090)
    {
        destroy_frontend_configuration_page_455090(f, page);
        return;
    }
    if (callback == 0x4557f0)
    {
        destroy_frontend_control_page_4557f0(f, page);
        return;
    }
    if (callback == 0x45ab70)
    {
        destroy_frontend_campaign_slot_page_45ab70(f, page);
        return;
    }
    if (callback == 0x459a20)
    {
        destroy_frontend_arcade_score_page_459a20(f, page);
        return;
    }
    if (callback == 0x45d2e0)
    {
        destroy_frontend_select_tree_page_45d2e0(f, page);
        return;
    }
    if (callback == 0x45b850)
    {
        destroy_frontend_select_team_page_45b850(f, page);
        return;
    }
    if (callback == 0x450e10)
    {
        destroy_frontend_campaign_menu_page_450e10(f, page);
        return;
    }
    if (callback == 0x4536f0)
    {
        destroy_frontend_modify_page_4536f0(f, page);
        return;
    }
    if (callback == 0x45a3f0)
    {
        destroy_frontend_campaign_score_page_45a3f0(f, page);
        return;
    }
    if (callback == 0x45be00)
    {
        destroy_frontend_text_splash_page_45be00(f, page);
        return;
    }
    if (callback == 0x45b5b0)
    {
        destroy_frontend_bmp_splash_page_45b5b0(f, page);
        return;
    }
    if (callback == 0x4529c0)
    {
        destroy_frontend_credits_page_4529c0(f, page);
        return;
    }
    if (callback == 0x454450)
    {
        destroy_frontend_enter_name_page_454450(f, page);
        return;
    }
    if (callback == 0x457500 || callback == 0x4570f0 || callback == 0x4572e0 || callback == 0x459300)
    {
        destroy_frontend_records_page_child(f, page);
        return;
    }
    if (callback == 0x45dde0)
    {
        destroy_frontend_unlock_page_45dde0(f, page);
        return;
    }
    if (callback == 0x452530)
    {
        destroy_frontend_catalog_page_452530(f, page);
        return;
    }
    if (callback == 0x4542a0)
    {
        destroy_frontend_multi_page_4542a0(f, page);
        return;
    }
    if (callback == 0x45a620)
        return; // entire PageSetup destructor; caller owns the free
    if (callback == 0x452d50)
    {
        destroy_frontend_language_page_452d50(f, page);
        return;
    }
    if (callback == 0x455410)
    {
        destroy_frontend_audio_options_455410(f, page);
        return;
    }
    // Also safe if the generic parent's later Save27 integration handles this
    // first: it returns there and this branch is then unreachable, not repeated.
    if (callback == 0x457ea0)
    {
        destroy_frontend_save_page_457ea0(f, page);
        return;
    }
    required_.close_remaining_page(f, page, callback);
}
void SourceMainMenuChildren::remaining_selection_page_event(FrontendState &f, Word page, Word callback, Word event,
                                                            Word argument)
{
    switch (callback)
    {
    case 0x45a410:
        (void)dispatch_frontend_campaign_score_page_45a410(f, page, event, *this, music_, *this, *this);
        return;
    case 0x453710:
        (void)dispatch_frontend_modify_page_453710(f, page, event, *this, music_, *this, *this, *this, *this);
        return;
    case 0x4512b0:
        (void)dispatch_frontend_campaign_menu_page_4512b0(f, page, event, *this, *this, music_, *this, *this);
        return;
    case 0x45b870:
        (void)dispatch_frontend_select_team_page_45b870(f, page, event, *this, music_, *this, *this, *this);
        return;
    case 0x45d320:
        (void)dispatch_frontend_select_tree_page_45d320(f, page, event, *this, music_, *this, *this, *this);
        return;
    case 0x459a40:
        (void)dispatch_frontend_arcade_score_page_459a40(f, page, event, *this, music_, *this, *this, *this, samples_);
        return;
    case 0x45abb0:
        (void)dispatch_frontend_campaign_slot_page_45abb0(f, page, event, *this, music_, *this, *this, *this);
        return;
    case 0x45bf40:
        (void)dispatch_frontend_text_splash_page_45bf40(f, page, event, *this, music_, *this, *this);
        return;
    case 0x45b5d0:
        (void)dispatch_frontend_bmp_splash_page_45b5d0(f, page, event, *this, music_, *this, *this);
        return;
    case 0x4550d0:
        (void)dispatch_frontend_configuration_page_4550d0(f, page, event, *this, *this, music_, *this);
        return;
    case 0x455800:
        (void)dispatch_frontend_control_page_455800(f, page, event, *this, music_, *this, *this, *this);
        return;
    case 0x4529e0:
        (void)dispatch_frontend_credits_page_4529e0(f, page, event, *this, music_, *this);
        return;
    case 0x454460:
        (void)dispatch_frontend_enter_name_page_454460(f, page, event, argument, *this, music_, *this, *this, *this);
        return;
    case 0x457520:
    case 0x457110:
    case 0x457300:
    case 0x459320:
        (void)dispatch_frontend_records_page(f, page, callback, event, *this, *this, music_, *this, *this);
        return;
    case 0x45de00:
        (void)dispatch_frontend_unlock_page_45de00(f, page, event, *this, music_, *this, *this);
        return;
    case 0x452550:
        (void)dispatch_frontend_catalog_page_452550(f, page, event, *this, music_, *this, *this);
        return;
    case 0x4542c0:
        (void)dispatch_frontend_multi_page_4542c0(f, page, event, *this, *this, music_, *this);
        return;
    case 0x45a630:
        (void)dispatch_frontend_setup_page_45a630(f, page, event, *this, *this, music_);
        return;
    case 0x452d90:
        (void)dispatch_frontend_language_page_452d90(f, page, event, *this, music_, *this, *this, *this);
        return;
    case 0x45a810:
        (void)dispatch_frontend_single_page_45a810(f, page, event, argument, *this, *this, music_, *this);
        return;
    case 0x452fe0:
        (void)dispatch_frontend_game_loop_452fe0(f, page, event, *this);
        return;
    case 0x452b60:
        (void)dispatch_frontend_start_page_452b60(f, page, event, *this, music_, counter_);
        return;
    case 0x455430:
        (void)dispatch_frontend_audio_options_455430(f, page, event, *this, music_, *this);
        return;
    case 0x457ec0:
        (void)dispatch_frontend_save_page_457ec0(f, page, event, *this, *this, music_, *this, save_);
        return;
    default:
        required_.remaining_page_event(f, page, callback, event, argument);
        return;
    }
}
void SourceMainMenuChildren::feedback_440880(FrontendState &f, Word feedback)
{
    source_menu_feedback_440880(f, feedback, samples_);
}
void SourceMainMenuChildren::feedback_440890(FrontendState &f, Word event)
{
    source_menu_feedback_440890(f, event, samples_);
}
void SourceMainMenuChildren::maintain_audio_462280(FrontendState &f)
{
    maintain_source_menu_audio_462280(f);
}
void SourceMainMenuChildren::prepare_camera_402e90(FrontendState &f)
{
    sgn::retail::prepare_camera_402e90(f);
}
void SourceMainMenuChildren::prepare_geometry_44fd60(FrontendState &f)
{
    sgn::retail::prepare_geometry_44fd60(f);
}
Word &SourceMainMenuChildren::demo_index_627fdc(FrontendState &f)
{
    return required_.demo_index_627fdc(f);
}
void SourceMainMenuChildren::load_demo_43b0e0(FrontendState &f, Word record)
{
    required_.load_demo_43b0e0(f, record);
}
void SourceMainMenuChildren::exit_462dfa(FrontendState &f, Word status)
{
    // NEW identity guard: another FrontendState may share a world but is not
    // the actual menu object whose callbacks and storage this Core published.
    if (&f != &frontend_)
        throw FormatError("Menu exit requires its actual Core frontend");
    // PageMain4532A1 calls462DFA directly. Do not insert fatal4234D0/442180
    // here: the NEW HOST must retire/clean up after SourceHostProcessExit.
    crt_.exit_menu(frontend_, shutdown_, status);
}
void SourceMainMenuChildren::run_mode(FrontendState &f, Word page, Word entry)
{
    if (entry == 0x44d4c0)
    {
        advance_frontend_stunt_challenge_mode_44d4c0(f, *this, *this, music_);
        return;
    }
    if (entry == 0x44c0a0)
    {
        advance_frontend_campaign_mode_44c0a0(f, *this, *this, music_);
        return;
    }
    if (entry == 0x44b5e0)
    {
        advance_frontend_championship_mode_44b5e0(f, *this, *this, music_);
        return;
    }
    if (entry == 0x44cd70)
    {
        advance_mode2_44cd70(f, *this, *this, music_, *this);
        return;
    }
    if (entry == 0x44d740)
    {
        advance_frontend_time_trial_mode_44d740(f, *this, *this, music_);
        return;
    }
    required_.run_remaining_mode(f, page, entry);
}
Word SourceMainMenuChildren::car_available_437230(FrontendState &f, Word car, Word mode)
{
    return source_car_available_437230(f, car, mode);
}
Word SourceMainMenuChildren::track_available_4371c0(FrontendState &f, Word track, Word mode)
{
    return source_track_available_4371c0(f, track, mode);
}
void SourceMainMenuChildren::rank_car_437cc0(FrontendState &f, Word car)
{
    f.shared_platform()->require_owner_thread();
    (void)source_car_class_437cc0(car); // source caller discards EAX; no rank store
}
Word SourceMainMenuChildren::format_name_46280c(FrontendState &f, FrontendLanguageTextIdentity format, Word argument,
                                                std::span<std::uint8_t, 64> output)
{
    auto platform = f.shared_platform();
    platform->require_owner_thread();
    const auto bytes = resolve_frontend_language_text(f.source.language_strings(), format);
    const std::array<Word, 1> arguments{argument};
    SourceScriptPathCall call;
    (void)format_selection_numbers_46280c(platform->state().boot_config, platform->source_application_literals(), bytes,
                                          arguments, output, 0x44cefa, call);
    return static_cast<Word>(call.initialized_prefix); // bounded by the supplied 64-byte output
}
void SourceMainMenuChildren::prepare_race_43a780(FrontendState &f, Word roster)
{
    prepare_frontend_mode2_race_43a780(f, roster);
}
void SourceMainMenuChildren::finish_race_43d500(FrontendState &f, Word key)
{
    request_frontend_save_page_43d500(f, key, *this, *this, music_);
}
Word SourceMainMenuChildren::replay_time_word_45811b(FrontendState &f, Word identity)
{
    return required_.replay_time_word_45811b(f, identity);
}
// Compile-time integration guard, not a claim that a build has been run.
static_assert(!std::is_abstract_v<SourceMainMenuChildren>);
} // namespace sgn::retail
