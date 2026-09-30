#include "sgn/retail_audio_driver.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace sgn::retail;
using sgn::Vec3;
namespace {
void check(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
struct StopBoundary final:std::exception {};
struct Device final:AudioDevice {
    std::vector<std::uint64_t> calls;
    std::function<AudioResult(AudioBufferId)> callback;
    AudioResult stop(AudioBufferId b) override {calls.push_back(b.value);return callback?callback(b):0;}
    [[noreturn]] static AudioResult unexpected(){throw std::runtime_error("unexpected non-Stop device operation");}
    AudioResult status(AudioBufferId,std::uint32_t&)override{return unexpected();}
    AudioResult duplicate(AudioBufferId,AudioBufferId&)override{return unexpected();}
    AudioResult query_spatial(AudioBufferId,AudioSpatialId&)override{return unexpected();}
    std::uint32_t release(AudioBufferId)override{return static_cast<std::uint32_t>(unexpected());}
    std::uint32_t release_spatial(AudioSpatialId)override{return static_cast<std::uint32_t>(unexpected());}
    AudioResult frequency(AudioBufferId,std::uint32_t)override{return unexpected();}
    AudioResult volume(AudioBufferId,std::int32_t)override{return unexpected();}
    AudioResult pan(AudioBufferId,std::int32_t)override{return unexpected();}
    AudioResult play(AudioBufferId,std::uint32_t,std::uint32_t,std::uint32_t)override{return unexpected();}
    AudioResult spatial_parameters(AudioSpatialId,const AudioSpatialParameters&,std::uint32_t)override{return unexpected();}
    AudioResult spatial_mode(AudioSpatialId,std::uint32_t,std::uint32_t)override{return unexpected();}
    AudioResult spatial_position(AudioSpatialId,Vec3,std::uint32_t)override{return unexpected();}
    AudioResult spatial_velocity(AudioSpatialId,Vec3,std::uint32_t)override{return unexpected();}
    AudioResult listener_position(AudioListenerId,Vec3,std::uint32_t)override{return unexpected();}
    AudioResult listener_velocity(AudioListenerId,Vec3,std::uint32_t)override{return unexpected();}
    AudioResult listener_orientation(AudioListenerId,Vec3,Vec3,std::uint32_t)override{return unexpected();}
};
struct Fixture {
    AudioDriverState state{};Device device;LevelAudioState level{};
    contact::player::Controls controls{};std::uint32_t split{};
    AudioDriver driver{state,device,level,controls,split};
};
}
int main()try {
    unsigned cases=0;
    {Fixture f;f.state.playing_count=37;f.state.samples[0].buffer={100};
        audio_stop_all(f.driver);check(f.device.calls.empty()&&f.state.playing_count==37,"uninitialized must skip");++cases;}
    {Fixture f;f.state.initialized=2;f.state.playing_count=0;
        audio_stop_all(f.driver);check(f.device.calls.empty()&&f.state.playing_count==0xffffff80u,"128 empty slots wrapping decrement");++cases;}
    {Fixture f;f.state.initialized=1;f.state.playing_count=200;
        for(unsigned i=0;i<128;++i)f.state.samples[i].buffer={1000u+i};
        f.device.callback=[](AudioBufferId){return -1;};audio_stop_all(f.driver);
        check(f.device.calls.size()==128&&f.state.playing_count==72,"HRESULT ignored and all128 visited");
        for(unsigned i=0;i<128;++i)check(f.device.calls[i]==1000u+i&&f.state.samples[i].buffer.value==1000u+i,"slot order and identities retained");++cases;}
    {Fixture f;f.state.initialized=1;f.state.samples[0].buffer={100};f.state.samples[1].buffer={101};
        f.device.callback=[&](AudioBufferId){f.state.initialized=0;f.state.playing_count=40;return 1;};
        audio_stop_all(f.driver);check(f.device.calls==std::vector<std::uint64_t>{100}&&f.state.playing_count==39,"initialized late reread and current decrement");++cases;}
    {Fixture f;f.state.initialized=1;f.state.playing_count=200;f.state.samples[0].buffer={100};f.state.samples[1].buffer={101};
        f.device.callback=[&](AudioBufferId b){if(b.value==100){f.state.samples[0].buffer={900};f.state.samples[1].buffer={901};}return 0;};
        audio_stop_all(f.driver);check(f.device.calls==std::vector<std::uint64_t>{100,901}&&f.state.playing_count==72&&f.state.samples[0].buffer.value==900,"captured current buffer and late next slot");++cases;}
    {Fixture f;f.state.initialized=1;f.state.playing_count=20;f.state.samples[1].buffer={100};std::uint32_t handle=1;
        f.device.callback=[&](AudioBufferId){handle=127;f.state.playing_count=81;return -1;};
        audio_stop_handle(f.driver,handle);check(handle==0&&f.state.playing_count==80&&f.device.calls==std::vector<std::uint64_t>{100},"handle source zero and late count");++cases;}
    {Fixture f;f.state.initialized=1;f.state.playing_count=20;f.state.samples[0].buffer={100};f.state.samples[1].buffer={101};
        f.device.callback=[&](AudioBufferId b)->AudioResult {if(b.value==101){f.state.playing_count=75;throw StopBoundary{};}return 0;};
        bool caught=false;try{audio_stop_all(f.driver);}catch(const StopBoundary&){caught=true;}
        check(caught&&f.device.calls==std::vector<std::uint64_t>{100,101}&&f.state.playing_count==75,"exception boundary preserves callback and prior prefix");++cases;}
    {Fixture f;f.state.initialized=1;f.state.playing_count=17;std::uint32_t handle=0;
        audio_stop_handle(f.driver,handle);check(f.device.calls.empty()&&handle==0&&f.state.playing_count==17,"null handle must not decrement");
        handle=127;audio_stop_handle(f.driver,handle);check(f.device.calls.empty()&&handle==0&&f.state.playing_count==16,"valid handle with empty buffer decrements");++cases;}
    std::cout<<"STOP_CALLBACK_PROBE_PASS cases="<<cases<<" production_changes=0 OS_runs=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
