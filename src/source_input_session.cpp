#include "sgn/source_input_dispatch.hpp"
#include "sgn/source_menu_input.hpp"
#include "sgn/source_force_feedback.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_pause.hpp"
#include <bit>
#include <utility>

namespace sgn::retail
{
namespace
{
SourcePhysicalInputView physical_view(RaceSessionState &s, SourceInputStorage &input, unsigned i)
{
    auto &raw = input.acquisition.physical[i];
    auto &extra = input.physical[i];
    return {s.menu_input.raw_words[i].word_0,
            raw.word_8,
            raw.word_0c,
            extra.activity_kind,
            extra.profile_index,
            s.frontend_launch.input_bindings[i],
            extra.feedback_mode,
            raw.word_24,
            raw.word_28,
            raw,
            extra.bytes_168_187,
            borrow_menu_input_bank(s.menu_input.edges[i], raw.extra_edges)};
}
SourceVehicleInputView vehicle_view(RaceSessionState &s, SourceInputStorage &input, unsigned i)
{
    auto &physical = s.vehicles.physics[i];
    auto &previous = s.vehicles.restart[i].previous_input;
    return {s.vehicles.startup[i].field_2280,
            physical.input,
            {previous.throttle, previous.brake, previous.left, previous.right, physical.previous_shift,
             physical.previous_stunt, previous.rear_lock, previous.steering_boost, previous.turbo, previous.auxiliary},
            input.vehicle_profiles[i],
            s.vehicles.race[i].control_kind,
            s.vehicles.parameters[i].steering_sensitivity};
}
template <std::size_t... I> auto physical_views(RaceSessionState &s, std::index_sequence<I...>)
{
    return std::array<SourcePhysicalInputView, 4>{physical_view(s, s.source_input, static_cast<unsigned>(I))...};
}
template <std::size_t... I> auto vehicle_views(RaceSessionState &s, std::index_sequence<I...>)
{
    return std::array<SourceVehicleInputView, 6>{vehicle_view(s, s.source_input, static_cast<unsigned>(I))...};
}
// Invocation-local references only. Storage declaration order keeps all spans
// valid. Both menu and race execute the same acquisition body below.
class SourceInputInvocation final
{
    RaceSessionState &s_;
    std::array<SourcePhysicalInputView, 4> physical_;
    std::array<SourceVehicleInputView, 6> vehicles_;
    std::array<MenuPhysicalInputView, 4> menu_physical_;
    SessionPauseInvocation pause_;
    SourceInputWorld world_;

  public:
    SourceInputInvocation(RaceSessionState &s, AudioDevice &audio)
        : s_(s), physical_(physical_views(s, std::make_index_sequence<4>{})),
          vehicles_(vehicle_views(s, std::make_index_sequence<6>{})),
          menu_physical_(menu_physical_producer_views(s.source_input.acquisition, s.menu_input)), pause_(s, audio),
          world_{s.frame_pause,
                 menu_keyboard_bytes(s.startup),
                 std::span<const std::uint8_t, 256>{
                     reinterpret_cast<const unsigned char *>(s.startup.fields_62dcf0.data()), 256},
                 physical_,
                 vehicles_,
                 s.source_input.profiles,
                 s.current_slot,
                 s.source_input.acquisition.source_device_pointer_576b6c,
                 s.normal_finish.confirm_pressed,
                 s.normal_finish.confirm_consumed,
                 s.pause.raw_activity,
                 s.source_input.previous_activity,
                 {s.scene_mode, pause_.lifecycle()},
                 {s.scene_mode, menu_keyboard_bytes(s.startup), menu_physical_,
                  source_menu_selectors(s.source_input.profiles),
                  s.source_input.acquisition.source_device_pointer_576b6c}}
    {
    }
    SourceInputInvocation(const SourceInputInvocation &) = delete;
    SourceInputInvocation &operator=(const SourceInputInvocation &) = delete;
    void acquire_45f210(MenuKeyboardAcquisition &keys, const MenuDirectInputRegistry &registry)
    {
        auto &s = s_;
        auto &raw = s.source_input.acquisition;
        // 45F210..45F248: original prefix and suffix, without 40DAD0.
        acquire_publish_devices_prefix_45f210(raw, s.menu_input, s.startup, s.ui.second_pause, keys, registry);
        publish_input_feedback_kind_45ef00(physical_); // 45F237
        update_source_input_activity_45f110(world_);   // 45F23C
        // 45F241 ->45EC20 is the one-byte RET.
        const std::array<PauseInputWordsView, 4> pause_words{
            {{s.menu_input.raw_words[0].word_0, s.menu_input.raw_words[0].word_4},
             {s.menu_input.raw_words[1].word_0, s.menu_input.raw_words[1].word_4},
             {s.menu_input.raw_words[2].word_0, s.menu_input.raw_words[2].word_4},
             {s.menu_input.raw_words[3].word_0, s.menu_input.raw_words[3].word_4}}};
        update_pause_input_45ee60({s.scene_mode, s.frontend_launch.input_bindings, pause_words,
                                   s.menu_input.previous_pause, pause_.lifecycle()}); // 45F248 tail
    }
    SourceInputResult dispatch_40dad0(SourceForceFeedbackWorld *feedback)
    {
        return dispatch_source_input_40dad0(world_, feedback);
    }
};
} // namespace
static SourceInputResult acquire_dispatch_impl(RaceSession &session, SourceInputStorage &input,
                                               MenuKeyboardAcquisition &keys, const MenuDirectInputRegistry &registry,
                                               SourceForceFeedbackWorld *feedback)
{
    session.shared_platform()->require_owner_thread();
    auto &s = session.state();
    if (&input != &s.source_input)
        throw FormatError("Session source input belongs to a different world");
    SourceInputInvocation invocation(s, session.audio_device());
    invocation.acquire_45f210(keys, registry);
    return invocation.dispatch_40dad0(feedback); // unchanged race 44281F child
}
void acquire_frontend_input_45f210(FrontendState &frontend, MenuKeyboardAcquisition &keys,
                                   Win32MenuDirectInputStartup &startup)
{
    auto &platform = *frontend.shared_platform();
    platform.require_owner_thread();
    auto &s = frontend.world();
    // Identity/lifetime validation only, as in the existing race adapter.
    (void)startup.borrow_force_feedback_state(s.source_input.acquisition);
    const auto &registry = startup.registry_for_acquisition();
    SourceInputInvocation invocation(s, platform.source_audio_owner());
    invocation.acquire_45f210(keys, registry);
}
SourceInputResult acquire_dispatch_source_platform_input(FrontendState &frontend, MenuKeyboardAcquisition &keys,
                                                         Win32MenuDirectInputStartup &startup)
{
    auto &platform = *frontend.shared_platform();
    platform.require_owner_thread();
    auto &s = frontend.world();
    if (!platform.source_construction_only_entry() || &s != &platform.state())
        throw FormatError("source race input requires the same frontend/platform world");
    (void)startup.borrow_force_feedback_state(s.source_input.acquisition);
    const auto &registry = startup.registry_for_acquisition();
    SourceForceFeedbackWorld feedback{platform, s.source_input, startup};
    SourceInputInvocation invocation(s, platform.source_audio_owner());
    invocation.acquire_45f210(keys, registry);
    return invocation.dispatch_40dad0(&feedback);
}
SourceInputResult acquire_dispatch_session_input(RaceSession &session, MenuKeyboardAcquisition &keys,
                                                 const MenuDirectInputRegistry &registry)
{
    session.shared_platform()->require_owner_thread();
    return acquire_dispatch_session_input(session, session.state().source_input, keys, registry);
}
SourceInputResult acquire_dispatch_session_input(RaceSession &session, SourceInputStorage &input,
                                                 MenuKeyboardAcquisition &keys, const MenuDirectInputRegistry &registry)
{
    return acquire_dispatch_impl(session, input, keys, registry, nullptr);
}
SourceInputResult acquire_dispatch_session_input(RaceSession &session, SourceInputStorage &input,
                                                 MenuKeyboardAcquisition &keys, Win32MenuDirectInputStartup &startup)
{
    session.shared_platform()->require_owner_thread();
    (void)startup.borrow_force_feedback_state(input.acquisition); // identity/lifetime only, no source writes
    const auto &registry = startup.registry_for_acquisition();
    SourceForceFeedbackWorld feedback{session, input, startup};
    return acquire_dispatch_impl(session, input, keys, registry, &feedback);
}
SourceInputResult acquire_dispatch_session_input(RaceSession &session, MenuKeyboardAcquisition &keys,
                                                 Win32MenuDirectInputStartup &startup)
{
    return acquire_dispatch_session_input(session, session.state().source_input, keys, startup);
}
} // namespace sgn::retail
