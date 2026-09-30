#include "source_audio_listener_native_configuration_os_receiving.hpp"
#include "sgn/race_session.hpp"
#include <windows.h>
#include <ostream>

namespace sgn::retail::receiving {
namespace {void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}}
SourceAudioListenerContinuationOutcome receive_listener_configuration_with_live_window(
    const SourceAudioListenerAcquisitionOutcome& acquired,RaceSessionPlatform& platform,InputWindowHandles handles,std::ostream& log){
    platform.require_owner_thread();
    const auto hwnd=reinterpret_cast<HWND>(handles.window);DWORD process{};
    require(hwnd&&::GetWindowThreadProcessId(hwnd,&process)==::GetCurrentThreadId()&&process==::GetCurrentProcessId()&&
        ::GetAncestor(hwnd,GA_ROOT)==hwnd&&static_cast<std::uintptr_t>(::GetWindowLongPtrW(hwnd,GWLP_HINSTANCE))==handles.instance,
        "A7 receiving requires the same retained local top-level window on its owner thread");
    (void)platform.window_input(handles.instance,handles.window);
    auto& world=platform.state();auto& owner=platform.source_audio_owner();
    const auto native_device=owner.source_device_identity(),native_primary=owner.source_primary_identity(),native_listener=owner.source_listener_identity();
    const auto ready=platform.host_readiness();const auto listener=world.audio_maintenance.listener;
    const auto device=world.audio_initialization.device;const auto primary=world.audio_initialization.primary;
    const auto rate=world.audio_initialization.requested_rate,capability=world.audio_initialization.spatial_capability;
    const auto enabled=world.audio_initialization.spatial_enabled,mode=world.audio_driver.spatial_mode;
    require(native_device&&native_primary&&!ready.audio_device&&!ready.audio_stream&&world.audio_driver.initialized==0,
        "A7 receiving requires the retained cold source A5/A6 owners before full initialization");
    const bool configuration=std::holds_alternative<SourceAudioListenerConfigurationBoundary>(acquired);
    auto outcome=continue_platform_source_audio_listener_461aa0(acquired,platform);
    const auto after=platform.host_readiness();
    require(owner.source_device_identity()==native_device&&owner.source_primary_identity()==native_primary&&
        owner.source_listener_identity()==native_listener&&world.audio_maintenance.listener==listener&&
        world.audio_initialization.device==device&&world.audio_initialization.primary==primary&&
        world.audio_initialization.requested_rate==rate&&world.audio_initialization.spatial_capability==capability&&
        world.audio_initialization.spatial_enabled==enabled&&world.audio_driver.spatial_mode==mode&&world.audio_driver.initialized==0&&
        ready.assets==after.assets&&ready.audio_device==after.audio_device&&ready.audio_stream==after.audio_stream&&
        ready.window_input==after.window_input,"A7 receiving observed a replaced owner, changed source state or full readiness");
    log<<"A7 configuration_executed="<<configuration<<" retained_listener="<<native_listener;
    if(std::holds_alternative<SourceAudioSamplesBoundary>(outcome))log<<" boundary=462182";
    else {const auto& failure=std::get<SourceAudioListenerAcquisitionFailure>(outcome);log<<" prior_A6_failure="<<failure.source_site;
        if(failure.query_result)log<<" prior_QI_HRESULT="<<*failure.query_result;}
    log<<" bridge_QI_calls=0 bridge_AddRef_calls=0 setter_HRESULTs_logged=0 sample_load=0 full_initialized=0\n";
    require(bool(log),"A7 receiving log failed");return outcome;
}
}
