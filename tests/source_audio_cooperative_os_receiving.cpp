#include "source_audio_cooperative_os_receiving.hpp"
#include "sgn/race_session.hpp"
#include <windows.h>
#include <iostream>

namespace sgn::retail::receiving {
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
CooperativeOsObservation receive_cooperative_with_live_window(
    const SourceAudioCooperativeLevelBoundary& reached,RaceSessionPlatform& platform,InputWindowHandles handles){
    platform.require_owner_thread();
    const auto window=reinterpret_cast<HWND>(handles.window);DWORD process{};
    require(window&&::GetWindowThreadProcessId(window,&process)==::GetCurrentThreadId()&&
        process==::GetCurrentProcessId()&&::GetAncestor(window,GA_ROOT)==window&&
        static_cast<std::uintptr_t>(::GetWindowLongPtrW(window,GWLP_HINSTANCE))==handles.instance,
        "receiving requires explicitly supplied live local top-level HWND/HINSTANCE on its message thread");
    auto& world=platform.state();auto& owner=platform.source_audio_owner();
    const auto device=world.audio_initialization.device;const auto primary=world.audio_initialization.primary;
    const auto enumeration=world.audio_initialization.enumeration;const auto listener=world.audio_maintenance.listener;
    const auto capability=world.audio_initialization.spatial_capability;const auto enabled=world.audio_initialization.spatial_enabled;
    const auto spatial=world.audio_driver.spatial_mode;const auto before=platform.host_readiness();
    require(device.value!=0&&device.value==owner.source_device_identity()&&!owner.host_backend_ready(),"fixture lacks the actual retained A2 source interface");
    const auto next=set_platform_source_audio_cooperative_462165(reached,platform,handles);
    const auto after=platform.host_readiness();
    require(world.audio_initialization.device==device&&owner.source_device_identity()==device.value&&
        world.audio_initialization.primary==primary&&&next.output==&world.audio_initialization.primary&&next.device==device,
        "cooperative stage replaced/released an owner or wrote the primary output before its call");
    require(world.audio_initialization.enumeration.records==enumeration.records&&world.audio_initialization.enumeration.cursor_word==enumeration.cursor_word&&
        world.audio_initialization.enumeration.count==enumeration.count&&world.audio_initialization.enumeration.adjacent_word==enumeration.adjacent_word&&
        world.audio_maintenance.listener==listener&&world.audio_initialization.spatial_capability==capability&&
        world.audio_initialization.spatial_enabled==enabled&&world.audio_driver.spatial_mode==spatial,"cooperative stage reset prior source writes");
    require(before.assets==after.assets&&before.audio_device==after.audio_device&&before.audio_stream==after.audio_stream&&before.window_input==after.window_input,
        "cooperative stage prepared/replaced a backend");
    const auto& request=next.request;
    require(request.size==20&&request.flags==0x11&&request.bytes==0&&request.reserved==0&&!request.format,"actual stage omitted source local descriptor prefix");
    bool stopped=false;try{next.resume();}catch(const SourceAudioDefaultDeviceError& e){stopped=e.source_site==0x461d25;}
    require(stopped&&owner.source_device_identity()==device.value,"next boundary not enforced or retained interface lost");
    const bool success=next.cooperative_result==0;
    std::cout<<"actual SetCooperativeLevel HWND="<<handles.window<<" level=3 HRESULT="<<next.cooperative_result
        <<" next=461D25 native_success="<<success<<'\n';
    return {next.cooperative_result,success};
}
}
