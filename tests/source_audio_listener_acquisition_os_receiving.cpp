#include "source_audio_listener_acquisition_os_receiving.hpp"
#include "sgn/race_session.hpp"
#include <windows.h>
#include <ostream>

namespace sgn::retail::receiving {
namespace {void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}}
SourceAudioListenerAcquisitionOutcome receive_listener_acquisition_with_live_window(
    const SourceAudioPrimaryFormatResult& reached,RaceSessionPlatform& platform,InputWindowHandles handles,std::ostream& log){
    platform.require_owner_thread();
    const auto hwnd=reinterpret_cast<HWND>(handles.window);DWORD process{};
    require(hwnd&&::GetWindowThreadProcessId(hwnd,&process)==::GetCurrentThreadId()&&process==::GetCurrentProcessId()&&
        ::GetAncestor(hwnd,GA_ROOT)==hwnd&&static_cast<std::uintptr_t>(::GetWindowLongPtrW(hwnd,GWLP_HINSTANCE))==handles.instance,
        "A6 receiving requires the retained owner-thread local top-level HWND");
    (void)platform.window_input(handles.instance,handles.window);
    auto& world=platform.state();auto& owner=platform.source_audio_owner();auto& audio=world.audio_initialization;
    require(reached.source_return()&&audio.device.value!=0&&audio.device.value==owner.source_device_identity()&&
        audio.primary.value!=0&&audio.primary.value==owner.source_primary_identity()&&owner.source_listener_identity()==0,
        "A6 receiving requires the actual same retained A5 device/primary before listener acquisition");
    const auto ready=platform.host_readiness();const auto device=audio.device;const auto primary=audio.primary;
    const auto rate=audio.requested_rate,capability=audio.spatial_capability,mode=world.audio_driver.spatial_mode;
    const auto prior_listener=world.audio_maintenance.listener;const auto initialized=world.audio_driver.initialized;
    require(!ready.audio_device&&!ready.audio_stream&&initialized==0,"A6 must not run on a prepared preview or initialized source world");
    auto outcome=acquire_platform_source_audio_listener_462174(reached,platform);
    const auto after=platform.host_readiness();
    require(audio.device==device&&audio.primary==primary&&owner.source_device_identity()==device.value&&
        owner.source_primary_identity()==primary.value&&audio.requested_rate==rate&&audio.spatial_capability==capability&&
        world.audio_driver.spatial_mode==mode&&world.audio_driver.initialized==initialized&&
        ready.assets==after.assets&&ready.audio_device==after.audio_device&&ready.audio_stream==after.audio_stream&&
        ready.window_input==after.window_input,"A6 replaced an owner or published later readiness/source state");
    log<<"A6 mode="<<mode<<" enabled="<<audio.spatial_enabled<<" prior_listener_word="<<prior_listener.value
        <<" listener_word="<<world.audio_maintenance.listener.value<<" native_listener="<<owner.source_listener_identity();
    if(const auto* configuration=std::get_if<SourceAudioListenerConfigurationBoundary>(&outcome)){
        require(&configuration->listener==&world.audio_maintenance.listener&&
            configuration->listener.value==owner.source_listener_identity(),"A6 configuration boundary lost the native source-slot alias");
        log<<" boundary=461AA0 query_HRESULT=0 acquired_nonnull="<<(owner.source_listener_identity()!=0);
    }else if(std::holds_alternative<SourceAudioSamplesBoundary>(outcome)){
        require(owner.source_listener_identity()==0&&world.audio_maintenance.listener==prior_listener,
            "A6 skipped branch invented a listener acquisition");log<<" boundary=462182 query_invoked=0";
    }else{
        const auto& failure=std::get<SourceAudioListenerAcquisitionFailure>(outcome);
        log<<" stopped="<<failure.source_site<<" query_invoked="<<failure.query_result.has_value();
        if(failure.query_result)log<<" query_HRESULT="<<*failure.query_result;
    }
    log<<" full_initialized="<<world.audio_driver.initialized<<" listener_configuration_executed=0\n";
    require(bool(log),"A6 receiving log failed");return outcome;
}
}
