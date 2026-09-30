#include "sgn/source_audio_default_device.hpp"
#include "sgn/race_session.hpp"
#include <atomic>
#include <iostream>
#include <thread>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Error,class F>void rejects(F&& call){
    try{call();}catch(const Error&){return;}
    throw std::runtime_error("expected owner/readiness rejection");
}
void no_host_backends(RaceSessionPlatform& platform){
    const auto ready=platform.host_readiness();
    require(!ready.assets&&!ready.audio_device&&!ready.audio_stream&&!ready.window_input,"source owner borrow prepared a host backend");
}
}
int main(){try{
    //A receiving fixture at the reached A1 boundary. This does not execute or
    //qualify the earlier462090 prefix, enumeration, or the complete cold parent.
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},
        std::filesystem::path("__unused_A2_source_receiving_root__"),0.0f);
    auto& world=platform->state();no_host_backends(*platform);
    require(world.audio_initialization.device.value==0,"cold source device prefix not zero");
    world.audio_initialization.enumeration.cursor_word=0x11000111;
    world.audio_initialization.enumeration.count=0x22000222;
    world.audio_initialization.enumeration.adjacent_word=0x33000333;
    world.audio_initialization.enumeration.records.fill(0xa9);
    world.audio_maintenance.listener.value=0x44000444;
    const auto enumeration=world.audio_initialization.enumeration;
    const auto listener=world.audio_maintenance.listener;
    const auto spatial=world.audio_driver.spatial_mode;
    auto* const device=&platform->prepare_source_audio_owner();
    require(&platform->prepare_source_audio_owner()==device,"second owner borrow replaced WindowsAudioDevice");
    no_host_backends(*platform);
    rejects<FormatError>([&]{(void)platform->audio_device();});
    const auto result=create_session_source_audio_default_device_462146({},*platform);
    const auto token=world.audio_initialization.device.value;
    require(token==device->source_device_identity(),"stage wrote a second source token/owner");
    require(world.audio_initialization.enumeration.cursor_word==enumeration.cursor_word&&
        world.audio_initialization.enumeration.count==enumeration.count&&
        world.audio_initialization.enumeration.adjacent_word==enumeration.adjacent_word&&
        world.audio_initialization.enumeration.records==enumeration.records&&
        world.audio_maintenance.listener==listener&&world.audio_driver.spatial_mode==spatial,"device stage reset enumeration/listener/spatial prefix");
    no_host_backends(*platform);
    if(std::holds_alternative<SourceAudioDefaultDeviceFailure>(result)){
        std::cerr<<"actual Session DirectSoundCreate failed HRESULT="<<std::get<SourceAudioDefaultDeviceFailure>(result).result
                 <<" token="<<token<<"; success receiving NOT qualified\n";return 2;
    }
    require(token!=0,"successful Session source creation fabricated null device");
    rejects<SourceAudioDefaultDeviceError>([&]{std::get<SourceAudioCooperativeLevelBoundary>(result).resume();});
    rejects<SourceAudioDefaultDeviceError>([&]{platform->prepare_host_audio();});
    require(&platform->prepare_source_audio_owner()==device&&device->source_device_identity()==token,"host preparation rejection replaced source interface");
    no_host_backends(*platform);
    {
        RaceSession session(SourceColdAttachmentOnly{},platform);
        require(&session.state()==&world&&session.shared_platform().get()==platform.get(),"cold Session copied native/world owner");
        rejects<FormatError>([&]{(void)session.audio_device();});
    }
    require(device->source_device_identity()==token&&world.audio_initialization.device.value==token,"Session destruction released persistent source interface");
    std::atomic<bool> rejected{};
    std::thread other([&]{try{(void)create_session_source_audio_default_device_462146({},*platform);}catch(const std::logic_error&){rejected=true;}});other.join();
    require(rejected&&world.audio_initialization.device.value==token,"foreign Session stage changed source token");
    const auto remaining=device->release_device(world.audio_initialization.device);
    require(!device->source_device_identity()&&world.audio_initialization.device.value==token,"source release invented4621ED store");
    world.audio_initialization.device={}; //Explicit receiving caller store.
    std::cout<<"Session A2 same-owner/deferred/lifetime receiving PASS; native Release="<<remaining<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
