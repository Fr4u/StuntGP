#include "sgn/source_audio_listener_acquisition.hpp"
#include "sgn/source_audio_default_device_native.hpp"
#include "listener_receiving_fixture.hpp"
#include "sgn/platform_audio.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void reject_at(F&& function,std::uint32_t site){
    try{function();}catch(const SourceAudioDefaultDeviceError& error){require(error.source_site==site,"wrong rejection PC");return;}
    throw std::runtime_error("missing source-owner admission rejection");
}
struct Marker {};
enum class Output {zero,owned,unchanged};
struct Listener;
struct Stats {
    HRESULT query_hr=S_OK,set_hr=S_OK;
    Output output=Output::owned;
    bool throw_before{},throw_after{},trap_foreign_release{};
    const std::thread::id owner_thread=std::this_thread::get_id();
    unsigned query_calls{},listener_live{},primary_live{},device_live{};
    unsigned listener_releases{},primary_releases{},device_releases{},adds{},unexpected{},bad_order{};
    std::uintptr_t listener_identity{},primary_identity{},device_identity{},incoming_word{};
    Listener* prepared_listener{};
    std::function<void()> during_query,during_set;
    std::vector<std::string_view> events;
    Stats(){events.reserve(32);}
};
struct Listener final:IDirectSound3DListener {
    Stats& s;ULONG references=0;
    explicit Listener(Stats& stats):s(stats){++s.listener_live;s.listener_identity=reinterpret_cast<std::uintptr_t>(this);}
    ~Listener(){--s.listener_live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override{++s.unexpected;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{++s.adds;return ++references;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(s.trap_foreign_release&&std::this_thread::get_id()!=s.owner_thread)std::_Exit(42);
        ++s.listener_releases;s.events.push_back("listener_release");if(s.primary_live!=1||s.device_live!=1)++s.bad_order;
        const auto remaining=--references;if(!remaining)delete this;return remaining;
    }
    HRESULT no_call(){++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetAllParameters(LPDS3DLISTENER)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetDistanceFactor(D3DVALUE*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetDopplerFactor(D3DVALUE*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetOrientation(D3DVECTOR*,D3DVECTOR*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetPosition(D3DVECTOR*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetRolloffFactor(D3DVALUE*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetVelocity(D3DVECTOR*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetAllParameters(LPCDS3DLISTENER,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetDistanceFactor(D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetDopplerFactor(D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetOrientation(D3DVALUE,D3DVALUE,D3DVALUE,D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetPosition(D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetRolloffFactor(D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetVelocity(D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE CommitDeferredSettings()override{return no_call();}
};
struct Primary final:IDirectSoundBuffer {
    Stats& s;ULONG references=1;
    explicit Primary(Stats& stats):s(stats){++s.primary_live;s.primary_identity=reinterpret_cast<std::uintptr_t>(this);}
    ~Primary(){--s.primary_live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{
        auto* const transfer=listener_receiving_fixture::begin_query(this);
        ++s.query_calls;s.events.push_back("query");
        //Independent bytes from PE46FE80, not an expectation copied from the implementation constant.
        constexpr std::array<unsigned char,16> expected{0x84,0xfa,0x9a,0x27,0x81,0x49,0xce,0x11,0xa5,0x21,0x00,0x20,0xaf,0x0b,0xe5,0x60};
        require(out&&std::memcmp(&iid,expected.data(),16)==0,"QI IID/output address differs from PE");
        s.incoming_word=reinterpret_cast<std::uintptr_t>(*out);
        if(s.throw_before)throw Marker{};
        if(s.output==Output::zero)*out=nullptr;
        else if(s.output==Output::owned){auto* value=s.prepared_listener?s.prepared_listener:new Listener(s);value->AddRef();*out=value;listener_receiving_fixture::report_listener_acquisition(transfer,value);}
        else if(s.query_hr==S_OK&&s.prepared_listener){s.prepared_listener->AddRef();listener_receiving_fixture::report_listener_acquisition(transfer,s.prepared_listener);}
        if(s.during_query)s.during_query();if(s.throw_after)throw Marker{};return s.query_hr;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{++s.adds;return ++references;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(s.trap_foreign_release&&std::this_thread::get_id()!=s.owner_thread)std::_Exit(42);
        ++s.primary_releases;s.events.push_back("primary_release");if(s.listener_live||s.device_live!=1)++s.bad_order;
        const auto remaining=--references;if(!remaining)delete this;return remaining;
    }
    HRESULT no_call(){++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD,LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX out,DWORD bytes,LPDWORD written)override{
        require(out&&bytes==18&&written,"A5 GetFormat setup differs");*out={1,2,44100,176400,4,16,0};*written=18;s.events.push_back("get");return S_OK;}
    HRESULT STDMETHODCALLTYPE GetVolume(LPLONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetPan(LPLONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND,LPCDSBUFFERDESC)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Lock(DWORD,DWORD,LPVOID*,LPDWORD,LPVOID*,LPDWORD,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Play(DWORD,DWORD,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX)override{s.events.push_back("set");if(s.during_set)s.during_set();return s.set_hr;}
    HRESULT STDMETHODCALLTYPE SetVolume(LONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetPan(LONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Stop()override{return no_call();}
    HRESULT STDMETHODCALLTYPE Unlock(LPVOID,DWORD,LPVOID,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Restore()override{return no_call();}
};
struct Device final:IDirectSound {
    Stats& s;ULONG references=1;
    explicit Device(Stats& stats):s(stats){++s.device_live;s.device_identity=reinterpret_cast<std::uintptr_t>(this);}
    ~Device(){--s.device_live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override{++s.unexpected;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{++s.adds;return ++references;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(s.trap_foreign_release&&std::this_thread::get_id()!=s.owner_thread)std::_Exit(42);
        ++s.device_releases;s.events.push_back("device_release");if(s.primary_live||s.listener_live)++s.bad_order;
        const auto remaining=--references;if(!remaining)delete this;return remaining;
    }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC d,LPDIRECTSOUNDBUFFER* out,LPUNKNOWN outer)override{
        require(d&&d->dwSize==sizeof(DSBUFFERDESC1)&&d->dwFlags==0x11&&out&&!*out&&!outer,"A4 setup differs");
        *out=new Primary(s);s.events.push_back("primary");return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS caps)override{require(caps&&caps->dwSize==96,"A5 caps setup differs");caps->dwFlags=8;caps->dwMaxHw3DAllBuffers=8;s.events.push_back("caps");return S_OK;}
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER,LPDIRECTSOUNDBUFFER*)override{++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD level)override{require(level==3,"A3 level differs");s.events.push_back("cooperative");return S_OK;}
    HRESULT STDMETHODCALLTYPE Compact()override{++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetSpeakerConfig(LPDWORD)override{++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD)override{++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(LPCGUID)override{++s.unexpected;return E_NOTIMPL;}
};
thread_local Stats* active=nullptr;
struct Scope {Stats* previous;explicit Scope(Stats& s):previous(active){active=&s;}~Scope(){active=previous;}};
HRESULT WINAPI create(LPCGUID guid,LPDIRECTSOUND* out,LPUNKNOWN outer){
    require(active&&out&&!*out&&!guid&&!outer,"A2 setup differs");*out=new Device(*active);active->events.push_back("create");return S_OK;}
const SourceAudioDefaultDeviceNativeApi api{&create,&listener_receiving_fixture::query_with_transfer};
struct Rig {
    Stats& s;Scope scope;
    std::unique_ptr<WindowsAudioDevice> owner;
    AudioDeviceId device;AudioPrimaryId primary;AudioListenerId listener{0x1234abcd};
    std::uint32_t mode=1,enabled=1,capability=1,rate=44100;
    SourceAudioPrimaryFormatResult formatted{0,0};
    explicit Rig(Stats& stats):s(stats),scope(stats),owner(std::make_unique<WindowsAudioDevice>(DeferredAudioConstructionOnly{},0.0f,api)){}
    void prepare(bool format=true){
        require(owner->create_source_default_device(device)==0,"A2 setup failed");
        require(owner->set_source_cooperative_level(device,1,3)==0,"A3 setup failed");
        require(owner->create_source_primary_buffer(device,{20,0x11,0,0,{}},primary)==0,"A4 setup failed");
        if(format)formatted=owner->complete_source_primary_format(device,primary,rate,capability,enabled);
    }
    SourceAudioListenerAcquisitionOutcome acquire(){return acquire_source_audio_listener_462174(formatted,*owner,primary,listener,mode,enabled);}
};
void released(const Stats& s,unsigned listener_refs){
    require(s.listener_releases==listener_refs&&s.primary_releases==1&&s.device_releases==1&&
        s.listener_live==0&&s.primary_live==0&&s.device_live==0&&s.bad_order==0&&s.unexpected==0,
        "release count/order or unexpected native configuration differs");
    require(s.adds==listener_refs,"bridge acquired an extra interface reference");
}
void mode_enabled_branch_matrix(){
    for(const auto mode:{0u,1u,0x80000000u,0xffffffffu})for(const auto enabled:{0u,1u,2u,0x80000000u,0xffffffffu}){
        Stats s;const bool query=mode!=0&&enabled==1;
        {Rig r(s);r.prepare();r.mode=mode;r.enabled=enabled;const auto incoming=r.listener.value;
            const auto result=r.acquire();require(r.mode==mode&&r.enabled==(mode==0?0u:enabled),"462174/46218E branch or write differs");
            require(r.rate==44100&&r.capability==1&&r.device.value==s.device_identity&&r.primary.value==s.primary_identity,"unrelated prefix changed");
            if(query){require(std::holds_alternative<SourceAudioListenerConfigurationBoundary>(result)&&s.query_calls==1&&r.listener.value==s.listener_identity,"enabled branch did not reach configuration");}
            else {require(std::holds_alternative<SourceAudioSamplesBoundary>(result)&&s.query_calls==0&&r.listener.value==incoming,"disabled branch called QI or changed listener");
                reject_at([&]{std::get<SourceAudioSamplesBoundary>(result).resume();},0x462182);}
            reject_at([&]{(void)r.acquire();},0x462174);
        }released(s,query?1u:0u);
    }
}
void hresult_output_and_input_word_matrix(){
    for(const HRESULT hr:{S_OK,S_FALSE,E_NOINTERFACE,E_FAIL})for(const Output output:{Output::zero,Output::owned,Output::unchanged})
    for(const auto incoming:{std::uint64_t{0},std::uint64_t{0x1234abcd}}){
        if(hr==S_OK&&output==Output::unchanged&&incoming)continue; //Not a valid COM success output.
        Stats s;s.query_hr=hr;s.output=output;const unsigned owned=output==Output::owned?1u:0u;
        {Rig r(s);r.prepare();r.listener.value=incoming;const auto result=r.acquire();
            require(s.incoming_word==incoming&&s.query_calls==1,"QI input word was pre-cleared or query repeated");
            const auto expected=output==Output::owned?s.listener_identity:(output==Output::zero?0:incoming);
            require(r.listener.value==expected&&r.owner->source_listener_identity()==(owned?s.listener_identity:0),"QI output publication or native ownership differs");
            if(hr==S_OK){const auto& boundary=std::get<SourceAudioListenerConfigurationBoundary>(result);require(&boundary.listener==&r.listener,"configuration lost source-slot alias");reject_at([&]{boundary.resume();},0x461aa0);}
            else{const auto& failure=std::get<SourceAudioListenerAcquisitionFailure>(result);require(failure.source_site==0x461a97&&failure.query_result==hr,"nonzero HRESULT was accepted or lost");}
        }released(s,owned);
    }
}
void same_value_success_reference(){
    Stats s;s.output=Output::unchanged;
    {Rig r(s);r.prepare();s.prepared_listener=new Listener(s);r.listener.value=s.listener_identity;
        const auto result=r.acquire();require(std::holds_alternative<SourceAudioListenerConfigurationBoundary>(result)&&r.owner->source_listener_identity()==s.listener_identity,"equal-valued successful QI reference was discarded");}
    released(s,1);
}
void source_null_and_mismatched_primary(){
    for(const bool zero:{true,false}){Stats s;{Rig r(s);r.prepare();r.primary.value=zero?0:r.primary.value+1;const auto incoming=r.listener.value;
        if(zero){const auto result=r.acquire();const auto& fail=std::get<SourceAudioListenerAcquisitionFailure>(result);require(fail.source_site==0x461a89&&!fail.query_result,"null primary fabricated a QI call/result");}
        else reject_at([&]{(void)r.acquire();},0x461a8a);
        require(s.query_calls==0&&r.listener.value==incoming&&r.enabled==1,"primary guard changed source prefix");}released(s,0);}
    for(const auto enabled:{0u,1u,2u}){Stats s;{Rig r(s);r.prepare();r.primary.value=0;r.mode=0;r.enabled=enabled;
        require(std::holds_alternative<SourceAudioSamplesBoundary>(r.acquire())&&r.enabled==0&&s.query_calls==0,"null primary inspected before mode branch");}released(s,0);}
}
void actual_format_history_and_late_reads(){
    for(const HRESULT set:{S_FALSE,E_FAIL}){Stats s;s.set_hr=set;{Rig r(s);r.prepare();
        reject_at([&]{(void)r.acquire();},0x46216d);r.formatted={0x461dea,0};reject_at([&]{(void)r.acquire();},0x462174);require(s.query_calls==0&&r.enabled==1,"forged A5 result advanced native history");}released(s,0);}
    {Stats s;{Rig r(s);r.prepare(false);r.formatted={0x461dea,0};reject_at([&]{(void)r.acquire();},0x462174);}released(s,0);}
    {Stats s;{Rig r(s);s.during_set=[] {throw Marker{};};bool caught=false;try{r.prepare();}catch(const Marker&){caught=true;}require(caught,"A5 exception missing");r.formatted={0x461dea,0};reject_at([&]{(void)r.acquire();},0x462174);}released(s,0);}
    {Stats s;{Rig r(s);s.during_set=[&]{r.mode=0;r.enabled=0xffffffff;r.device.value=123;};r.prepare();require(std::holds_alternative<SourceAudioSamplesBoundary>(r.acquire())&&r.enabled==0&&r.device.value==123,"mode was snapshotted or unused device word was consulted");}released(s,0);}
    {Stats s;{Rig r(s);r.prepare();s.during_query=[&]{r.mode=77;r.enabled=88;r.capability=99;r.primary.value=123;};
        require(std::holds_alternative<SourceAudioListenerConfigurationBoundary>(r.acquire())&&r.mode==77&&r.enabled==88&&r.capability==99&&r.primary.value==123,"QI callback writes were rolled back or primary re-read too early");}released(s,1);}
}
void exception_publication_and_reentry(){
    for(const bool before:{true,false})for(const Output output:{Output::zero,Output::owned,Output::unchanged}){
        Stats s;s.throw_before=before;s.throw_after=!before;s.output=output;
        {Rig r(s);r.prepare();const std::uint64_t incoming=r.listener.value;bool caught=false;try{(void)r.acquire();}catch(const Marker&){caught=true;}
            require(caught,"native receiving exception disappeared");const auto expected=before||output==Output::unchanged?incoming:(output==Output::zero?0:s.listener_identity);
            require(r.listener.value==expected&&r.owner->source_listener_identity()==(!before&&output==Output::owned?s.listener_identity:0),"exception lost native write prefix or adopted GUID data");
            reject_at([&]{(void)r.acquire();},0x462174);
        }released(s,!before&&output==Output::owned?1u:0u);
    }
    Stats s;{Rig r(s);r.prepare();unsigned blocked=0;s.during_query=[&]{
        AudioDeviceId other;AudioPrimaryId p;
        try{(void)r.owner->create_source_default_device(other);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)r.owner->set_source_cooperative_level(r.device,1,3);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)r.owner->create_source_primary_buffer(r.device,{20,0x11,0,0,{}},p);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)r.owner->complete_source_primary_format(r.device,r.primary,r.rate,r.capability,r.enabled);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)r.acquire();}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)r.owner->release_device(r.device);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{r.owner->prepare_host_backend();}catch(const SourceAudioDefaultDeviceError&){++blocked;}
    };require(std::holds_alternative<SourceAudioListenerConfigurationBoundary>(r.acquire())&&blocked==7,"QI allowed reentrant owner/native operations");}released(s,1);
}
void thread_and_retained_release_boundaries(){
    Stats s;{Rig r(s);r.prepare();const auto incoming=r.listener.value;std::atomic<bool> refused=false;
        std::thread worker([&]{try{(void)r.acquire();}catch(const SourceAudioDefaultDeviceError&){refused=true;}});worker.join();
        require(refused&&s.query_calls==0&&r.listener.value==incoming,"foreign thread reached QI or changed source word");
        (void)r.acquire();reject_at([&]{(void)r.owner->release_device(r.device);},0x4621ea);
        require(s.listener_releases==0&&s.primary_releases==0&&s.device_releases==0&&!r.owner->host_backend_ready(),"bounded source release or preview mixing occurred");
    }released(s,1);
}
struct DeathContext {bool armed{};const WindowsAudioDevice* expected{};const WindowsAudioDevice* deleting{};const Stats* stats{};std::uintptr_t device{},primary{},listener{};};
thread_local DeathContext death;
[[noreturn]]void fail_terminate()noexcept{std::_Exit(41);}
[[noreturn]]void listener_terminate()noexcept{
    const auto& c=death;const auto* s=c.stats;
    if(c.armed&&c.expected&&c.expected==c.deleting&&s&&s->trap_foreign_release&&std::this_thread::get_id()!=s->owner_thread&&
        c.device&&c.primary&&c.listener&&c.device==s->device_identity&&c.primary==s->primary_identity&&c.listener==s->listener_identity&&
        s->device_live==1&&s->primary_live==1&&s->listener_live==1&&s->device_releases==0&&s->primary_releases==0&&s->listener_releases==0)std::_Exit(0);
    std::_Exit(41);
}
int foreign_destroy(std::string_view mode){
    std::set_terminate(fail_terminate);if(mode=="--death-before-setup")std::terminate();
    Stats s;s.trap_foreign_release=true;Rig r(s);r.prepare();(void)r.acquire();const auto* expected=r.owner.get();
    require(r.device.value==s.device_identity&&r.primary.value==s.primary_identity&&r.listener.value==s.listener_identity&&r.listener.value,"death fixture missing retained listener");
    std::set_terminate(listener_terminate);if(mode=="--death-main")std::terminate();auto* raw=r.owner.release();
    std::thread worker([&,raw]{
        std::set_terminate(listener_terminate);if(mode=="--death-unarmed-worker")std::terminate();
        death={true,expected,raw,&s,r.device.value,r.primary.value,r.listener.value};
        if(mode=="--death-wrong-listener"){++death.listener;std::terminate();}
        if(mode=="--death-wrong-primary"){++death.primary;std::terminate();}
        if(mode=="--death-wrong-device"){++death.device;std::terminate();}
        if(mode=="--death-wrong-object"){death.deleting=nullptr;std::terminate();}
        if(mode=="--death-other-worker"){std::thread other([]{std::set_terminate(listener_terminate);std::terminate();});other.join();}
        delete raw;std::_Exit(1);
    });worker.join();return 1;
}
}
int main(int argc,char** argv){try{
    if(argc==2){const std::string_view mode=argv[1];if(mode=="--foreign-destroy-listener"||mode=="--death-before-setup"||mode=="--death-main"||mode=="--death-unarmed-worker"||mode=="--death-wrong-listener"||mode=="--death-wrong-primary"||mode=="--death-wrong-device"||mode=="--death-wrong-object"||mode=="--death-other-worker")return foreign_destroy(mode);return 2;}
    if(argc!=1)return 2;
    mode_enabled_branch_matrix();hresult_output_and_input_word_matrix();same_value_success_reference();source_null_and_mismatched_primary();
    actual_format_history_and_late_reads();exception_publication_and_reentry();thread_and_retained_release_boundaries();
    std::cout<<"source listener acquisition: 7 groups, 20 branch cases, 23 HRESULT/output/input cases PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
