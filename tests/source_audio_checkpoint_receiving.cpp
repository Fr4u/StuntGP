#include "source_audio_checkpoint_receiving.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_audio_primary_format.hpp"
#include <windows.h>
#include <ostream>
#include <stdexcept>
#include <variant>

namespace sgn::retail::receiving {
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void verify_attachment(RaceSession& session,RaceSessionPlatform& platform,InputWindowHandles handles){
    platform.require_owner_thread();
    const auto hwnd=reinterpret_cast<HWND>(handles.window);DWORD process{};
    require(hwnd&&::GetWindowThreadProcessId(hwnd,&process)==::GetCurrentThreadId()&&
        process==::GetCurrentProcessId()&&::GetAncestor(hwnd,GA_ROOT)==hwnd&&
        static_cast<std::uintptr_t>(::GetWindowLongPtrW(hwnd,GWLP_HINSTANCE))==handles.instance,
        "checkpoint requires the supplied local top-level HWND/HINSTANCE on its owner thread");
    require(platform.source_construction_only_entry()&&
        session.readiness()==RaceSessionReadiness::source_attachment_only&&
        &session.state()==&platform.state()&&session.shared_platform().get()==&platform&&
        !session.shared_state().owner_before(session.shared_platform())&&
        !session.shared_platform().owner_before(session.shared_state()),
        "checkpoint requires the same cold Session/world/platform control block");
    (void)platform.window_input(handles.instance,handles.window);
    const auto ready=platform.host_readiness();
    require(!ready.assets&&!ready.audio_device&&!ready.audio_stream&&ready.window_input&&
        platform.state().audio_driver.initialized==0,
        "checkpoint prepared preview resources or published audio readiness");
}
}
AudioCheckpointObservation receive_source_audio_checkpoint(
    RaceSession& session,InputWindowHandles handles,std::ostream& log){
    auto& platform=*session.shared_platform();
    verify_attachment(session,platform,handles);
    auto& world=platform.state();auto& audio=world.audio_initialization;
    require(audio.device.value==0&&audio.primary.value==0,"checkpoint cannot reset an existing source device/primary");
    const auto rate=audio.requested_rate,spatial=world.audio_driver.spatial_mode;
    const auto input=world.audio_level.input_b;
    AudioCheckpointObservation observation;
    const auto finish=[&]{
        verify_attachment(session,platform,handles);
        require(audio.requested_rate==rate&&world.audio_level.input_b==input&&
            world.audio_driver.spatial_mode==spatial,"checkpoint changed CFG rate/input or ran later spatial-mode branch");
        log<<"CHECKPOINT last_call="<<observation.last_call<<" HRESULT="<<observation.result
            <<" cooperative_HRESULT="<<observation.cooperative_result
            <<" gate="<<observation.input_gate_open<<" completed_461CF0="<<observation.primary_format_completed
            <<" initialized="<<world.audio_driver.initialized<<" rate="<<audio.requested_rate
            <<" capability="<<audio.spatial_capability<<" enabled="<<audio.spatial_enabled
            <<" enumeration_count="<<audio.enumeration.count<<" device="<<audio.device.value
            <<" primary="<<audio.primary.value<<" listener_word="<<world.audio_maintenance.listener.value<<'\n';
        require(bool(log),"checkpoint log failed");
        return observation;
    };
    log<<"AUTHORED_SOURCE_AUDIO_CHAIN HWND="<<handles.window<<" HINSTANCE="<<handles.instance
        <<" input_b="<<input<<" requested_rate="<<rate<<'\n';
    observation.last_call=0x4620fdu;
    observation.input_gate_open=initialize_audio_prefix_462090(audio,world.audio_driver,world.audio_maintenance,world.audio_level);
    if(!observation.input_gate_open)return finish();
    observation.last_call=0x46210eu;
    const auto enumeration=enumerate_session_source_audio_46210e(platform);
    if(const auto* failure=std::get_if<SourceAudioEnumerationFailure>(&enumeration)){
        observation.result=failure->result;return finish();
    }
    observation.last_call=0x462146u;
    const auto creation=create_session_source_audio_default_device_462146(
        std::get<SourceAudioDefaultDeviceBoundary>(enumeration),platform);
    if(const auto* failure=std::get_if<SourceAudioDefaultDeviceFailure>(&creation)){
        observation.result=failure->result;return finish();
    }
    observation.last_call=0x462165u;
    const auto cooperative=set_platform_source_audio_cooperative_462165(
        std::get<SourceAudioCooperativeLevelBoundary>(creation),platform,handles);
    observation.cooperative_result=cooperative.cooperative_result;
    // The original caller deliberately ignores cooperative HRESULT.
    observation.last_call=0x461d25u;
    const auto primary=create_platform_source_audio_primary_461d25(cooperative,platform);
    if(const auto* failure=std::get_if<SourceAudioPrimaryFailure>(&primary)){
        observation.result=failure->result;return finish();
    }
    const auto format=complete_platform_source_audio_primary_format_461d4c(
        std::get<SourceAudioDeviceCapsBoundary>(primary),platform);
    observation.last_call=format.last_call;observation.result=format.result;
    observation.primary_format_completed=format.source_return();
    return finish();
}
}
