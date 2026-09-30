#include "source_audio_primary_format_os_receiving.hpp"
#include "sgn/race_session.hpp"
#include <windows.h>
#include <iostream>

namespace sgn::retail::receiving {
namespace {void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}}
SourceAudioPrimaryFormatResult receive_primary_format_with_live_window(
    const SourceAudioDeviceCapsBoundary& reached,RaceSessionPlatform& platform,InputWindowHandles handles){
    platform.require_owner_thread();const auto hwnd=reinterpret_cast<HWND>(handles.window);DWORD process{};
    require(hwnd&&::GetWindowThreadProcessId(hwnd,&process)==::GetCurrentThreadId()&&process==::GetCurrentProcessId()&&
        ::GetAncestor(hwnd,GA_ROOT)==hwnd&&static_cast<std::uintptr_t>(::GetWindowLongPtrW(hwnd,GWLP_HINSTANCE))==handles.instance,
        "format receiving requires supplied live local top-level HWND/HINSTANCE on its owner thread");
    (void)platform.window_input(handles.instance,handles.window);
    auto& world=platform.state();auto& owner=platform.source_audio_owner();auto& audio=world.audio_initialization;
    require(reached.device==audio.device&&&reached.primary==&audio.primary&&audio.device.value!=0&&audio.primary.value!=0&&
        audio.device.value==owner.source_device_identity()&&audio.primary.value==owner.source_primary_identity(),
        "format receiving requires retained actual A4 device/primary and same world output");
    const auto device=audio.device;const auto primary=audio.primary;const auto enumeration=audio.enumeration;
    const auto listener=world.audio_maintenance.listener;const auto spatial=world.audio_driver.spatial_mode;
    const auto initialized=world.audio_driver.initialized;const auto rate=audio.requested_rate;
    const auto capability=audio.spatial_capability;const auto enabled=audio.spatial_enabled;
    const auto before=platform.host_readiness();
    const auto result=complete_platform_source_audio_primary_format_461d4c(reached,platform);
    const auto after=platform.host_readiness();
    require(audio.device==device&&audio.primary==primary&&owner.source_device_identity()==device.value&&
        owner.source_primary_identity()==primary.value,"format setup replaced/released actual native owners");
    require(audio.enumeration.records==enumeration.records&&audio.enumeration.cursor_word==enumeration.cursor_word&&
        audio.enumeration.count==enumeration.count&&audio.enumeration.adjacent_word==enumeration.adjacent_word&&
        world.audio_maintenance.listener==listener&&world.audio_driver.spatial_mode==spatial&&world.audio_driver.initialized==initialized&&
        audio.requested_rate==rate,"format setup changed unrelated source state or declared initialized");
    require((audio.spatial_capability==capability&&audio.spatial_enabled==enabled)||
        (audio.spatial_capability==0&&audio.spatial_enabled==0),"spatial state is neither retained nor source-cleared");
    if(result.last_call==0x461d4cu)require(audio.spatial_capability==capability&&audio.spatial_enabled==enabled,
        "failed caps call altered source capability words");
    require(before.assets==after.assets&&before.audio_device==after.audio_device&&before.audio_stream==after.audio_stream&&
        before.window_input==after.window_input&&!owner.host_backend_ready(),"format setup prepared a backend");
    require(result.last_call==0x461d4cu||result.last_call==0x461d7eu||result.last_call==0x461deau,"unknown returned source call site");
    require(result.result!=0||result.last_call==0x461deau,"zero result returned before final source call");
    std::cout<<"actual primary setup HWND="<<handles.window<<" last_call="<<result.last_call<<" HRESULT="<<result.result
        <<" source_return="<<result.source_return()<<" requested_rate="<<rate<<" physical_format_equality=UNVERIFIED\n";
    return result;
}
}
