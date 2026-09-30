#include "sgn/source_audio_primary.hpp"
#include "sgn/source_audio_default_device_native.hpp"
#include "sgn/platform_audio.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
template<class F>void reject(F&& f){
    try{f();}catch(const SourceAudioDefaultDeviceError&){return;}
    throw std::runtime_error("missing bounded admission rejection");
}
struct Marker {int value;};
struct Stats {
    HRESULT create_hr=S_OK,cooperative_hr=S_OK,primary_hr=S_OK;
    bool publish=true;
    bool trap_foreign_release=false;
    const std::thread::id owner_thread{std::this_thread::get_id()};
    unsigned device_live{},primary_live{},device_releases{},primary_releases{},adds{},other{},calls{},bad_release_order{};
    std::uintptr_t device_identity{},primary_identity{};
    std::vector<std::string_view> events;
    std::function<void()> cooperative_during,before_output,after_output;
    Stats(){events.reserve(64);}
};
struct Primary final:IDirectSoundBuffer {
    Stats& s;ULONG refs=1;
    explicit Primary(Stats& stats):s(stats){++s.primary_live;s.primary_identity=reinterpret_cast<std::uintptr_t>(this);}
    ~Primary(){--s.primary_live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** p)override{++s.other;if(p)*p=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{++s.adds;return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(s.trap_foreign_release&&std::this_thread::get_id()!=s.owner_thread)std::_Exit(32);
        ++s.primary_releases;s.events.push_back("primary_release");if(s.device_live!=1)++s.bad_release_order;
        const auto n=--refs;if(!n)delete this;return n;
    }
    HRESULT unexpected(){++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD,LPDWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX,DWORD,LPDWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetVolume(LPLONG)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetPan(LPLONG)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND,LPCDSBUFFERDESC)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Lock(DWORD,DWORD,LPVOID*,LPDWORD,LPVOID*,LPDWORD,DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Play(DWORD,DWORD,DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE SetVolume(LONG)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE SetPan(LONG)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Stop()override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Unlock(LPVOID,DWORD,LPVOID,DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Restore()override{return unexpected();}
};
struct Device final:IDirectSound {
    Stats& s;ULONG refs=1;
    explicit Device(Stats& stats):s(stats){++s.device_live;s.device_identity=reinterpret_cast<std::uintptr_t>(this);}
    ~Device(){--s.device_live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** p)override{++s.other;if(p)*p=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{++s.adds;return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(s.trap_foreign_release&&std::this_thread::get_id()!=s.owner_thread)std::_Exit(32);
        ++s.device_releases;s.events.push_back("device_release");if(s.primary_live)++s.bad_release_order;
        const auto n=--refs;if(!n)delete this;return n;
    }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC d,LPDIRECTSOUNDBUFFER* out,LPUNKNOWN outer)override{
        ++s.calls;s.events.push_back("primary");
        require(d&&out&&!*out&&!outer,"unexpected primary descriptor/output/aggregation");
        require(d->dwSize==sizeof(DSBUFFERDESC1)&&d->dwSize==16+sizeof(void*),"native descriptor version or pointer size is wrong");
        require(d->dwFlags==(DSBCAPS_PRIMARYBUFFER|DSBCAPS_CTRL3D)&&d->dwBufferBytes==0&&d->dwReserved==0&&
            d->lpwfxFormat==nullptr,"native primary fields differ from source");
        if(s.before_output)s.before_output();
        if(s.publish)*out=new Primary(s);
        if(s.after_output)s.after_output();
        return s.primary_hr;
    }
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER,LPDIRECTSOUNDBUFFER*)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD level)override{
        require(level==3,"wrong cooperative level");s.events.push_back("cooperative");
        if(s.cooperative_during)s.cooperative_during();return s.cooperative_hr;
    }
    HRESULT STDMETHODCALLTYPE Compact()override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetSpeakerConfig(LPDWORD)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD)override{++s.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(LPCGUID)override{++s.other;return E_NOTIMPL;}
};
thread_local Stats* active=nullptr;
struct Scope {Stats* before;explicit Scope(Stats& s):before(active){active=&s;}~Scope(){active=before;}};
HRESULT WINAPI create(LPCGUID guid,LPDIRECTSOUND* out,LPUNKNOWN outer){
    auto& s=*active;require(!guid&&!outer&&out&&!*out,"wrong DirectSoundCreate args");
    s.events.push_back("create");*out=new Device(s);return s.create_hr;
}
const SourceAudioDefaultDeviceNativeApi api{&create};
AudioBufferRequest exact_request(){return {20,0x11,0,0,{}};}
SourceAudioPrimaryBufferBoundary reach(WindowsAudioDevice& owner,AudioDeviceId& id,AudioPrimaryId& output){
    const auto a2=create_source_audio_default_device_462146({},id,owner);
    return set_source_audio_cooperative_462165(std::get<SourceAudioCooperativeLevelBoundary>(a2),id,output,1,owner);
}
void hresult_output_matrix_and_next_boundary(){
    for(const HRESULT cooperative:{S_OK,S_FALSE,E_FAIL})for(const HRESULT primary:{S_OK,S_FALSE,E_FAIL})for(const bool publish:{false,true}){
        Stats s;s.cooperative_hr=cooperative;s.primary_hr=primary;s.publish=publish;Scope scope(s);
        AudioDeviceId id;AudioPrimaryId output;
        {
            WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);const auto a3=reach(owner,id,output);
            const auto result=create_source_audio_primary_461d25(a3,owner);
            require(s.calls==1&&output.value==s.primary_identity&&owner.source_primary_identity()==s.primary_identity,
                "native output was lost, truncated or not retained");
            require(id.value==s.device_identity&&owner.source_device_identity()==s.device_identity&&a3.cooperative_result==cooperative,
                "same device identity/cooperative observation changed");
            require(!owner.host_backend_ready()&&s.adds==0&&s.other==0&&s.device_releases==0&&s.primary_releases==0,
                "premature release/AddRef or unqualified later API");
            if(primary==S_OK){
                require(std::holds_alternative<SourceAudioDeviceCapsBoundary>(result),"zero HRESULT did not reach GetCaps");
                const auto& next=std::get<SourceAudioDeviceCapsBoundary>(result);
                require(next.device==id&&&next.primary==&output&&next.requested_size==96,"caps prefix or primary owner copied");
                bool stopped=false;try{next.resume();}catch(const SourceAudioDefaultDeviceError& e){stopped=e.source_site==0x461d4c;}
                require(stopped,"did not stop at device GetCaps");
            }else require(std::get<SourceAudioPrimaryFailure>(result).result==primary,"nonzero HRESULT was normalized to success");
            reject([&]{(void)create_source_audio_primary_461d25(a3,owner);});
            require(s.calls==1,"reused boundary silently repeated creation");
        }
        require(s.device_releases==1&&s.primary_releases==unsigned(publish)&&s.device_live==0&&s.primary_live==0&&
            s.bad_release_order==0,"host destruction leaked, double released or released parent first");
        std::vector<std::string_view> events{"create","cooperative","primary"};
        if(publish)events.push_back("primary_release");events.push_back("device_release");
        require(s.events==events,"native call order changed");
        require(output.value==s.primary_identity,"host cleanup invented a source output reset");
    }
}
void descriptor_and_output_admission(){
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId output;
    const auto a3=reach(owner,id,output);std::array<AudioBufferRequest,5> bad;
    bad.fill(exact_request());bad[0].size=24;bad[1].flags=1;bad[2].bytes=16;bad[3].reserved=1;bad[4].format=AudioWaveFormat{};
    for(const auto& r:bad)reject([&]{(void)owner.create_source_primary_buffer(id,r,output);});
    output.value=0x123456789abcdef0ull;const auto sentinel=output.value;
    reject([&]{(void)create_source_audio_primary_461d25(a3,owner);});
    require(output.value==sentinel&&s.calls==0,"invalid descriptor/output was changed or reached native API");
    output.value=0;
    reject([&]{(void)owner.create_source_primary_buffer({id.value+1},exact_request(),output);});
    reject([&]{(void)owner.create_source_primary_buffer({},exact_request(),output);});
    require(s.calls==0,"mismatched device reached primary API");
    (void)create_source_audio_primary_461d25(a3,owner);require(s.calls==1,"rejection consumed actual cooperative completion");
}
void actual_cooperative_completion_required(){
    for(const HRESULT result:{S_FALSE,E_FAIL}){
        Stats s;s.create_hr=result;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId output;
        (void)create_source_audio_default_device_462146({},id,owner);
        reject([&]{(void)owner.create_source_primary_buffer(id,exact_request(),output);});
        require(s.calls==0,"nonnull failed default device authorized primary");
    }
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId output;
    reject([&]{(void)owner.create_source_primary_buffer({},exact_request(),output);});
    (void)create_source_audio_default_device_462146({},id,owner);
    const SourceAudioPrimaryBufferBoundary forged{0,id,output,exact_request()};
    reject([&]{(void)create_source_audio_primary_461d25(forged,owner);});
    s.cooperative_during=[] {throw Marker{17};};bool caught=false;
    try{(void)set_source_audio_cooperative_462165({},id,output,1,owner);}catch(const Marker& m){caught=m.value==17;}
    require(caught,"cooperative exception swallowed");
    reject([&]{(void)create_source_audio_primary_461d25(forged,owner);});
    require(s.calls==0,"incomplete cooperative call authorized primary");
    s.cooperative_during={};s.cooperative_hr=E_FAIL;
    (void)set_source_audio_cooperative_462165({},id,output,1,owner);
    s.cooperative_during=[] {throw Marker{18};};caught=false;
    try{(void)set_source_audio_cooperative_462165({},id,output,1,owner);}catch(const Marker& m){caught=m.value==18;}
    require(caught,"second cooperative exception swallowed");
    reject([&]{(void)create_source_audio_primary_461d25(forged,owner);});
    require(s.calls==0,"prior completed cooperative call survived a later incomplete call");
    s.cooperative_during={};
    const auto next=set_source_audio_cooperative_462165({},id,output,1,owner);
    (void)create_source_audio_primary_461d25(next,owner);
    require(s.calls==1,"completed negative cooperative HRESULT prevented source continuation");
}
void exceptions_preserve_output_and_release_order(){
    for(const bool after_write:{false,true}){
        Stats s;Scope scope(s);AudioDeviceId id;AudioPrimaryId output;
        {
            WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);const auto a3=reach(owner,id,output);
            auto& hook=after_write?s.after_output:s.before_output;hook=[] {throw Marker{51};};bool caught=false;
            try{(void)create_source_audio_primary_461d25(a3,owner);}catch(const Marker& m){caught=m.value==51;}
            require(caught&&output.value==s.primary_identity&&owner.source_primary_identity()==s.primary_identity&&
                s.primary_releases==0&&s.device_releases==0,"exception discarded output or performed failure cleanup");
            reject([&]{(void)create_source_audio_primary_461d25(a3,owner);});require(s.calls==1,"exception allowed implicit retry");
        }
        require(s.primary_releases==unsigned(after_write)&&s.device_releases==1&&s.bad_release_order==0,
            "exception path native lifetime is incorrect");
    }
}
void reentrancy_and_post_primary_shutdown_boundary(){
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId output;
    const auto a3=reach(owner,id,output);unsigned blocked=0;
    s.after_output=[&]{
        require(owner.source_primary_identity()==s.primary_identity,"API did not write the retained owner slot directly");
        try{(void)create_source_audio_primary_461d25(a3,owner);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)owner.release_device(id);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)owner.set_source_cooperative_level(id,1,3);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        AudioDeviceId other;
        try{(void)owner.create_source_default_device(other);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
    };
    (void)create_source_audio_primary_461d25(a3,owner);require(blocked==4&&s.calls==1,"reentrant call/parent release was allowed");
    const auto token=output.value;reject([&]{(void)owner.release_device(id);});
    require(output.value==token&&s.primary_releases==0&&s.device_releases==0,"bounded shutdown guard changed native/source state");
}
void foreign_thread_and_prior_release(){
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId output;
    const auto a3=reach(owner,id,output);std::atomic<bool> refused{};
    std::thread worker([&]{try{(void)create_source_audio_primary_461d25(a3,owner);}catch(const SourceAudioDefaultDeviceError&){refused=true;}});
    worker.join();require(refused&&s.calls==0&&output.value==0,"foreign thread reached primary API");
    const auto token=id.value;require(owner.release_device(id)==0&&id.value==token,"pre-primary release changed source token/refcount");
    reject([&]{(void)create_source_audio_primary_461d25(a3,owner);});require(s.calls==0,"released device reached primary API");
}
//Death receiving defaults to failure. Only the exact armed foreign delete
//with both still-retained interfaces and no native Release can return zero.
struct DeathContext {
    bool armed{};const WindowsAudioDevice* expected{};const WindowsAudioDevice* deleting{};
    const Stats* stats{};std::uintptr_t device{},primary{};
};
thread_local DeathContext death;
[[noreturn]] void fail_terminate()noexcept{std::_Exit(31);}
[[noreturn]] void primary_terminate()noexcept{
    const auto& c=death;const auto* s=c.stats;
    if(c.armed&&c.expected&&c.expected==c.deleting&&s&&s->trap_foreign_release&&
        std::this_thread::get_id()!=s->owner_thread&&c.device&&c.primary&&
        c.device==s->device_identity&&c.primary==s->primary_identity&&s->device_live==1&&s->primary_live==1&&
        s->device_releases==0&&s->primary_releases==0)std::_Exit(0);
    std::_Exit(31);
}
int foreign_primary_destroy(std::string_view mode){
    std::set_terminate(fail_terminate);
    if(mode=="--death-before-setup")std::terminate();
    Stats s;s.trap_foreign_release=true;Scope scope(s);AudioDeviceId id;AudioPrimaryId output;
    auto owner=std::make_unique<WindowsAudioDevice>(DeferredAudioConstructionOnly{},0.0f,api);
    const auto a3=reach(*owner,id,output);(void)create_source_audio_primary_461d25(a3,*owner);
    const auto* expected=owner.get();const auto device=id.value;const auto primary=output.value;
    require(device==s.device_identity&&primary==s.primary_identity&&device&&primary,"death fixture lacks actual retained outputs");
    std::set_terminate(primary_terminate);
    if(mode=="--death-main")std::terminate();
    auto* raw=owner.release();
    std::thread worker([&,raw]{
        std::set_terminate(primary_terminate);
        if(mode=="--death-unarmed-worker")std::terminate();
        death={true,expected,raw,&s,device,primary};
        if(mode=="--death-wrong-primary"){++death.primary;std::terminate();}
        if(mode=="--death-wrong-object"){death.deleting=nullptr;std::terminate();}
        if(mode=="--death-other-worker"){
            std::thread other([]{std::set_terminate(primary_terminate);std::terminate();});other.join();
        }
        delete raw;
        std::_Exit(1); //Destructor returned, so the affinity guard did not work.
    });
    worker.join();return 1;
}
}
int main(int argc,char** argv){try{
    if(argc==2){
        const std::string_view mode=argv[1];
        if(mode=="--foreign-destroy-primary"||mode=="--death-before-setup"||mode=="--death-main"||
            mode=="--death-unarmed-worker"||mode=="--death-wrong-primary"||mode=="--death-wrong-object"||mode=="--death-other-worker")
            return foreign_primary_destroy(mode);
        return 2;
    }
    if(argc!=1)return 2;
    hresult_output_matrix_and_next_boundary();descriptor_and_output_admission();actual_cooperative_completion_required();
    exceptions_preserve_output_and_release_order();reentrancy_and_post_primary_shutdown_boundary();foreign_thread_and_prior_release();
    std::cout<<"source primary: 6 ABI/ordering groups, 18 HRESULT/output combinations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
