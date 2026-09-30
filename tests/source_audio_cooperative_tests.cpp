#include "sgn/source_audio_cooperative.hpp"
#include "sgn/source_audio_default_device_native.hpp"
#include "sgn/platform_audio.hpp"
#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>void reject(F&& f){
    try{f();}catch(const SourceAudioDefaultDeviceError&){return;}
    throw std::runtime_error("missing source admission rejection");
}
struct Marker {int value;};
struct Stats {
    HRESULT creation=S_OK,cooperative=S_OK;unsigned creates{},live{},releases{},adds{},other{};
    std::vector<std::pair<std::uintptr_t,DWORD>> calls;
    std::function<void()> during;
};
struct Native final:IDirectSound {
    Stats& s;ULONG refs=1;
    explicit Native(Stats& stats):s(stats){++s.live;}
    ~Native(){--s.live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out)override{++s.other;if(out)*out=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{++s.adds;return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{++s.releases;const auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC,LPDIRECTSOUNDBUFFER*,LPUNKNOWN)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER,LPDIRECTSOUNDBUFFER*)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd,DWORD level)override{
        s.calls.emplace_back(reinterpret_cast<std::uintptr_t>(hwnd),level);
        if(s.during)s.during();return s.cooperative;
    }
    HRESULT STDMETHODCALLTYPE Compact()override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetSpeakerConfig(LPDWORD)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(LPCGUID)override{++s.other;return E_NOTIMPL;}
};
thread_local Stats* active=nullptr;
struct Scope {Stats* previous;explicit Scope(Stats& s):previous(active){active=&s;}~Scope(){active=previous;}};
HRESULT WINAPI create(LPCGUID guid,LPDIRECTSOUND* out,LPUNKNOWN outer){
    auto& s=*active;require(!guid&&!outer&&out&&!*out,"unexpected source creation arguments");
    ++s.creates;*out=new Native(s);return s.creation;
}
const SourceAudioDefaultDeviceNativeApi api{&create};
void exact_arguments_ignored_results_and_boundary(){
    for(const HRESULT result:{S_OK,S_FALSE,E_FAIL}){
        Stats s;s.cooperative=result;Scope scope(s);AudioDeviceId id;AudioPrimaryId primary{0x77889900};
        {
            WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);
            const auto a2=create_source_audio_default_device_462146({},id,owner);
            constexpr auto window=static_cast<std::uintptr_t>(0x123456789abcdef0ull);
            const auto boundary=set_source_audio_cooperative_462165(std::get<SourceAudioCooperativeLevelBoundary>(a2),id,primary,window,owner);
            require(s.calls==std::vector<std::pair<std::uintptr_t,DWORD>>({{window,3}}),"source HWND/level altered or method repeated");
            require(boundary.cooperative_result==static_cast<AudioResult>(result)&&boundary.source_site==0x461d25,"ignored HRESULT became a source failure branch");
            require(boundary.device==id&&&boundary.output==&primary&&primary.value==0x77889900,"next call output/identity was copied or reset");
            const auto& r=boundary.request;require(r.size==20&&r.flags==0x11&&r.bytes==0&&r.reserved==0&&!r.format,"461CF0 local descriptor prefix incorrect");
            reject([&]{boundary.resume();});
            require(s.releases==0&&s.adds==0&&s.other==0&&s.live==1&&owner.source_device_identity()==id.value&&!owner.host_backend_ready(),"cooperative/boundary performed premature cleanup or later native call");
        }
        require(s.releases==1&&s.live==0,"source host cleanup did not release exactly once");
    }
}
void native_error_is_not_prevalidated(){
    Stats s;s.cooperative=E_INVALIDARG;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
    const auto a2=create_source_audio_default_device_462146({},id,owner);
    const auto next=set_source_audio_cooperative_462165(std::get<SourceAudioCooperativeLevelBoundary>(a2),id,primary,0,owner);
    require(s.calls.size()==1&&s.calls[0].first==0&&next.cooperative_result==static_cast<AudioResult>(E_INVALIDARG),"low-level HWND failure was replaced by host health/IsWindow behavior");
    reject([&]{(void)owner.set_source_cooperative_level(id,1,2);});
    reject([&]{(void)owner.set_source_cooperative_level(id,1,4);});
    require(s.calls.size()==1,"non-source level reached interface");
}
void failed_creation_cannot_continue(){
    for(const HRESULT failed:{S_FALSE,E_FAIL}){
        Stats s;s.creation=failed;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
        const auto a2=create_source_audio_default_device_462146({},id,owner);
        require(std::holds_alternative<SourceAudioDefaultDeviceFailure>(a2)&&id.value!=0,"fixture did not retain failed output");
        //A forged aggregate boundary cannot turn retained failure output into
        //the successful46214D branch. Same owner validates actual call history.
        reject([&]{(void)set_source_audio_cooperative_462165({},id,primary,1,owner);});
        require(s.calls.empty()&&s.releases==0&&s.live==1,"nonnull failure output authorized continuation or was released");
    }
}
void identity_and_release_order(){
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
    const auto a2=create_source_audio_default_device_462146({},id,owner);const auto token=id.value;
    reject([&]{(void)owner.set_source_cooperative_level({token+1},1,3);});
    reject([&]{(void)owner.set_source_cooperative_level({},1,3);});
    const auto next=set_source_audio_cooperative_462165(std::get<SourceAudioCooperativeLevelBoundary>(a2),id,primary,1,owner);
    require(s.calls.size()==1&&s.releases==0,"valid retained interface was not used");
    require(owner.release_device(id)==0&&id.value==token,"native release invented source token reset");
    reject([&]{(void)set_source_audio_cooperative_462165({},id,primary,1,owner);});
    require(s.calls.size()==1&&s.releases==1&&s.live==0,"released interface was called or double released");
    reject([&]{next.resume();});
}
void exception_and_reentrancy(){
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary{42};
    const auto a2=create_source_audio_default_device_462146({},id,owner);
    s.during=[] {throw Marker{93};};bool caught=false;
    try{(void)set_source_audio_cooperative_462165(std::get<SourceAudioCooperativeLevelBoundary>(a2),id,primary,1,owner);}catch(const Marker& m){caught=m.value==93;}
    require(caught&&s.releases==0&&primary.value==42,"exception rolled back source/native state");
    unsigned rejects=0;
    s.during=[&]{
        try{(void)owner.release_device(id);}catch(const SourceAudioDefaultDeviceError&){++rejects;}
        try{(void)owner.set_source_cooperative_level(id,1,3);}catch(const SourceAudioDefaultDeviceError&){++rejects;}
    };
    (void)set_source_audio_cooperative_462165(std::get<SourceAudioCooperativeLevelBoundary>(a2),id,primary,1,owner);
    require(rejects==2&&s.calls.size()==2&&s.live==1&&s.releases==0,"native call permitted reentrant release/call or exception left lock set");
    s.during={};require(owner.release_device(id)==0,"call activity flag retained after return");
}
void wrong_thread_and_empty_owner(){
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
    reject([&]{(void)owner.set_source_cooperative_level({},1,3);});
    const auto a2=create_source_audio_default_device_462146({},id,owner);std::atomic<bool> rejected{};
    std::thread other([&]{try{(void)set_source_audio_cooperative_462165(std::get<SourceAudioCooperativeLevelBoundary>(a2),id,primary,1,owner);}catch(const SourceAudioDefaultDeviceError&){rejected=true;}});other.join();
    require(rejected&&s.calls.empty()&&s.releases==0,"foreign thread reached native interface");
}
}
int main(){try{
    exact_arguments_ignored_results_and_boundary();native_error_is_not_prevalidated();failed_creation_cannot_continue();
    identity_and_release_order();exception_and_reentrancy();wrong_thread_and_empty_owner();
    std::cout<<"source cooperative: 6 ABI/ordering groups PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
