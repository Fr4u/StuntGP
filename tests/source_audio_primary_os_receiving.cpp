#include "source_audio_primary_os_receiving.hpp"
#include "sgn/race_session.hpp"
#include <windows.h>
#include <mmreg.h>
#include <dsound.h>
#include <iostream>

namespace sgn::retail::receiving {
namespace {void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}}
PrimaryOsObservation receive_primary_with_live_window(
    const SourceAudioPrimaryBufferBoundary& reached,RaceSessionPlatform& platform,InputWindowHandles handles){
    platform.require_owner_thread();
    const auto hwnd=reinterpret_cast<HWND>(handles.window);DWORD process{};
    require(hwnd&&::GetWindowThreadProcessId(hwnd,&process)==::GetCurrentThreadId()&&process==::GetCurrentProcessId()&&
        ::GetAncestor(hwnd,GA_ROOT)==hwnd&&static_cast<std::uintptr_t>(::GetWindowLongPtrW(hwnd,GWLP_HINSTANCE))==handles.instance,
        "receiving requires the explicitly supplied live local top-level HWND/HINSTANCE on its owner thread");
    (void)platform.window_input(handles.instance,handles.window);
    auto& world=platform.state();auto& owner=platform.source_audio_owner();
    const auto device=world.audio_initialization.device;
    require(device==reached.device&&device.value!=0&&owner.source_device_identity()==device.value&&
        &reached.output==&world.audio_initialization.primary&&reached.output.value==0&&owner.source_primary_identity()==0,
        "receiving requires the same A2/A3 source owner and unwritten primary output");
    const auto enumeration=world.audio_initialization.enumeration;
    const auto listener=world.audio_maintenance.listener;
    const auto capability=world.audio_initialization.spatial_capability;
    const auto enabled=world.audio_initialization.spatial_enabled;
    const auto spatial=world.audio_driver.spatial_mode;
    const auto before=platform.host_readiness();
    const auto outcome=create_platform_source_audio_primary_461d25(reached,platform);
    const auto after=platform.host_readiness();
    require(world.audio_initialization.device==device&&owner.source_device_identity()==device.value&&
        world.audio_initialization.primary.value==owner.source_primary_identity(),"native owner or output identity was replaced");
    require(world.audio_initialization.enumeration.records==enumeration.records&&
        world.audio_initialization.enumeration.cursor_word==enumeration.cursor_word&&world.audio_initialization.enumeration.count==enumeration.count&&
        world.audio_initialization.enumeration.adjacent_word==enumeration.adjacent_word&&world.audio_maintenance.listener==listener&&
        world.audio_initialization.spatial_capability==capability&&world.audio_initialization.spatial_enabled==enabled&&
        world.audio_driver.spatial_mode==spatial,"primary creation reset source state or ran later capability/listener logic");
    require(before.assets==after.assets&&before.audio_device==after.audio_device&&before.audio_stream==after.audio_stream&&
        before.window_input==after.window_input&&!owner.host_backend_ready(),"primary creation prepared a preview backend");
    AudioResult hr{};bool reached_caps=false;
    if(const auto* failure=std::get_if<SourceAudioPrimaryFailure>(&outcome)){
        hr=failure->result;require(hr!=0,"failure alternative carries zero HRESULT");
    }else{
        const auto& next=std::get<SourceAudioDeviceCapsBoundary>(outcome);reached_caps=true;
        require(next.device==device&&&next.primary==&world.audio_initialization.primary&&next.requested_size==96,"incorrect next GetCaps boundary");
        bool stopped=false;try{next.resume();}catch(const SourceAudioDefaultDeviceError& e){stopped=e.source_site==0x461d4c;}
        require(stopped,"GetCaps boundary is not enforced");
    }
    const bool success=hr==0&&world.audio_initialization.primary.value!=0;
    std::cout<<"actual CreateSoundBuffer HWND="<<handles.window<<" source_size=20 native_size="<<sizeof(DSBUFFERDESC1)
        <<" cooperative_HRESULT="<<reached.cooperative_result<<" HRESULT="<<hr<<" primary="<<world.audio_initialization.primary.value
        <<" reached_461D4C="<<reached_caps<<" native_success="<<success<<'\n';
    return {hr,reached_caps,success};
}
}
