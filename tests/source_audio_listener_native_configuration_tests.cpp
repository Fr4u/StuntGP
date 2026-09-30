#include "sgn/source_audio_listener_native_configuration.hpp"
#include <bit>
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
struct SetterCall {unsigned kind;std::uint32_t bits;DWORD apply;std::uintptr_t receiver;};
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
    std::array<HRESULT,3> setter_results{S_OK,S_OK,S_OK};
    std::array<std::function<void()>,3> during_setter;
    std::vector<SetterCall> setter_calls;
    int throwing_setter=-1;
    Stats(){events.reserve(32);setter_calls.reserve(3);}
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
    HRESULT setter(unsigned kind,float factor,DWORD apply){
        s.setter_calls.push_back({kind,std::bit_cast<std::uint32_t>(factor),apply,reinterpret_cast<std::uintptr_t>(this)});
        if(s.during_setter[kind])s.during_setter[kind]();
        if(s.throwing_setter==static_cast<int>(kind))throw Marker{};
        return s.setter_results[kind];
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
    HRESULT STDMETHODCALLTYPE SetDistanceFactor(D3DVALUE factor,DWORD apply)override{return setter(2,factor,apply);}
    HRESULT STDMETHODCALLTYPE SetDopplerFactor(D3DVALUE factor,DWORD apply)override{return setter(1,factor,apply);}
    HRESULT STDMETHODCALLTYPE SetOrientation(D3DVALUE,D3DVALUE,D3DVALUE,D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetPosition(D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetRolloffFactor(D3DVALUE factor,DWORD apply)override{return setter(0,factor,apply);}
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
    explicit Rig(Stats& stats,SourceAudioDefaultDeviceNativeApi native=api):s(stats),scope(stats),owner(std::make_unique<WindowsAudioDevice>(DeferredAudioConstructionOnly{},0.0f,native)){}
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

template<class F>void configuration_error(F&& function,std::uint32_t site){
    try{function();}catch(const SourceAudioListenerConfigurationError& error){require(error.source_site==site,"wrong late listener dereference PC");return;}
    throw std::runtime_error("missing late listener guard");
}
void checked_calls(const Stats& s,unsigned expected){
    //Independent source PUSH words and SDK slots15/12/11, not adapter defaults.
    constexpr std::array<std::uint32_t,3> bits{0x3f800000u,0x40c00000u,0x3c23d70au};
    require(s.setter_calls.size()==expected,"setter prefix length differs");
    for(unsigned n=0;n<expected;++n){const auto& call=s.setter_calls[n];
        require(call.kind==n&&call.bits==bits[n]&&call.apply==0&&call.receiver==s.listener_identity,
            "wrong SDK method/order/factor/apply or a second listener owner");}
    require(s.query_calls==1&&s.adds==1,"configuration repeated QI or acquired another reference");
}
void native_hresult_matrix(){
    constexpr std::array<HRESULT,5> results{S_OK,S_FALSE,E_FAIL,E_INVALIDARG,static_cast<HRESULT>(0x80000000u)};
    for(const auto a:results)for(const auto b:results)for(const auto c:results){
        Stats s;s.setter_results={a,b,c};
        {Rig r(s);r.prepare();const auto acquired=r.acquire();const auto listener=r.listener;
            const auto result=continue_source_audio_listener_461aa0(acquired,*r.owner);
            require(std::holds_alternative<SourceAudioSamplesBoundary>(result)&&r.listener==listener,
                "returned HRESULT stopped the source continuation or rewrote listener");
            checked_calls(s,3);reject_at([&]{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);},0x461aa0);
        }released(s,1);
    }
}
void late_pointer_matrix(){
    constexpr std::array<std::uint32_t,3> sites{0x461aadu,0x461abeu,0x461ad0u};
    for(int after=-1;after<3;++after)for(const bool zero:{true,false}){
        Stats s;{Rig r(s);r.prepare();const auto acquired=r.acquire();
            const auto change=[&]{r.listener.value=zero?0:s.listener_identity+1;};
            if(after<0)change();else s.during_setter[static_cast<std::size_t>(after)]=change;
            if(after==2)require(std::holds_alternative<SourceAudioSamplesBoundary>(continue_source_audio_listener_461aa0(acquired,*r.owner)),"invented a listener reload after third setter");
            else configuration_error([&]{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);},sites[static_cast<std::size_t>(after+1)]);
            const auto count=static_cast<unsigned>(after+1);checked_calls(s,count);
            require(r.listener.value==(zero?0:s.listener_identity+1)&&r.owner->source_listener_identity()==s.listener_identity,
                "late source write was rolled back or native owner was replaced");
            reject_at([&]{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);},0x461aa0);
        }released(s,1);
    }
}
void successful_null_query_is_late(){
    for(const auto overwrite:{std::uint64_t{0},std::uint64_t{123}}){
        Stats s;s.output=Output::zero;
        {Rig r(s);r.prepare();const auto acquired=r.acquire();r.listener.value=overwrite;
            configuration_error([&]{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);},0x461aad);
            require(s.setter_calls.empty()&&s.query_calls==1&&r.listener.value==overwrite,"null QI was eagerly rejected or reached native setter");
        }released(s,0);
    }
}
void acquisition_history_and_pass_through(){
    for(const HRESULT hr:{S_FALSE,E_FAIL}){Stats s;s.query_hr=hr;
        {Rig r(s);r.prepare();const auto acquired=r.acquire();const auto result=continue_source_audio_listener_461aa0(acquired,*r.owner);
            const auto& failure=std::get<SourceAudioListenerAcquisitionFailure>(result);
            require(failure.source_site==0x461a97&&failure.query_result==hr&&s.setter_calls.empty(),"A6 failure did not pass through unchanged");
            const SourceAudioListenerAcquisitionOutcome forged=SourceAudioListenerConfigurationBoundary{r.listener};
            reject_at([&]{(void)continue_source_audio_listener_461aa0(forged,*r.owner);},0x461aa0);
        }released(s,1);
    }
    for(const bool mode_zero:{true,false}){Stats s;
        {Rig r(s);r.prepare();if(mode_zero)r.mode=0;else r.enabled=0;const auto before=r.listener;const auto acquired=r.acquire();
            require(std::holds_alternative<SourceAudioSamplesBoundary>(continue_source_audio_listener_461aa0(acquired,*r.owner))&&
                s.query_calls==0&&s.setter_calls.empty()&&r.listener==before,"skipped QI became configuration");
            const SourceAudioListenerAcquisitionOutcome forged=SourceAudioListenerConfigurationBoundary{r.listener};
            reject_at([&]{(void)continue_source_audio_listener_461aa0(forged,*r.owner);},0x461aa0);
        }released(s,0);
    }
    {Stats s;{Rig r(s);r.prepare();r.primary.value=0;const auto acquired=r.acquire();const auto result=continue_source_audio_listener_461aa0(acquired,*r.owner);
        const auto& failure=std::get<SourceAudioListenerAcquisitionFailure>(result);require(failure.source_site==0x461a89&&!failure.query_result&&s.setter_calls.empty()&&s.query_calls==0,"null primary failure invented calls/results");}released(s,0);}
    {Stats s;s.throw_after=true;{Rig r(s);r.prepare();bool caught=false;try{(void)r.acquire();}catch(const Marker&){caught=true;}require(caught,"A6 receiving exception missing");
        const SourceAudioListenerAcquisitionOutcome forged=SourceAudioListenerConfigurationBoundary{r.listener};
        reject_at([&]{(void)continue_source_audio_listener_461aa0(forged,*r.owner);},0x461aa0);require(s.setter_calls.empty(),"throwing QI admitted configuration");}released(s,1);}
}
void same_slot_and_thread_admission(){
    Stats s;{Rig r(s);r.prepare();const auto acquired=r.acquire();AudioListenerId copied=r.listener;
        const SourceAudioListenerAcquisitionOutcome wrong_slot=SourceAudioListenerConfigurationBoundary{copied};
        reject_at([&]{(void)continue_source_audio_listener_461aa0(wrong_slot,*r.owner);},0x461aa0);
        require(s.setter_calls.empty(),"equal-valued copied listener slot was accepted");
        std::atomic<bool> refused=false;
        std::thread worker([&]{try{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);}catch(const SourceAudioDefaultDeviceError&){refused=true;}});worker.join();
        require(refused&&s.setter_calls.empty(),"foreign thread reached native setters");
        require(std::holds_alternative<SourceAudioSamplesBoundary>(continue_source_audio_listener_461aa0(acquired,*r.owner)),"invalid caller consumed legitimate owner-thread history");checked_calls(s,3);
    }released(s,1);
}
void exceptions_keep_prefix(){
    for(int at=0;at<3;++at){Stats s;s.throwing_setter=at;
        {Rig r(s);r.prepare();const auto acquired=r.acquire();const auto before=r.rate;
            for(auto& callback:s.during_setter)callback=[&]{++r.rate;};
            bool caught=false;try{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);}catch(const Marker&){caught=true;}
            require(caught&&r.rate==before+static_cast<unsigned>(at+1),"native exception was swallowed or earlier writes reverted");
            checked_calls(s,static_cast<unsigned>(at+1));reject_at([&]{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);},0x461aa0);
        }released(s,1);
    }
}
void reentry_and_unrelated_source_writes(){
    Stats s;{Rig r(s);r.prepare();const auto acquired=r.acquire();unsigned blocked=0,writes=0;
        for(auto& callback:s.during_setter)callback=[&]{
            ++writes;r.mode=0;r.enabled=0;r.capability=77;r.rate=123;r.device.value=222;r.primary.value=333;
            AudioDeviceId other;AudioPrimaryId primary;
            try{(void)r.owner->create_source_default_device(other);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            try{(void)r.owner->set_source_cooperative_level(r.device,1,3);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            try{(void)r.owner->create_source_primary_buffer(r.device,{20,0x11,0,0,{}},primary);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            try{(void)r.owner->complete_source_primary_format(r.device,r.primary,r.rate,r.capability,r.enabled);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            try{(void)r.acquire();}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            try{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            try{(void)r.owner->release_device(r.device);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            try{r.owner->prepare_host_backend();}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        };
        require(std::holds_alternative<SourceAudioSamplesBoundary>(continue_source_audio_listener_461aa0(acquired,*r.owner))&&blocked==24&&writes==3,
            "setter allowed reentrant mutations or reran source mode/enabled gates");
        checked_calls(s,3);require(r.mode==0&&r.enabled==0&&r.capability==77&&r.rate==123&&r.device.value==222&&r.primary.value==333,
            "configuration consulted or rewrote unrelated source globals");
        require(!r.owner->host_backend_ready()&&r.owner->source_listener_identity()==s.listener_identity,"configuration mixed preview/native owners");
    }released(s,1);
}
}
int main(){try{
    native_hresult_matrix();late_pointer_matrix();successful_null_query_is_late();acquisition_history_and_pass_through();
    same_slot_and_thread_admission();exceptions_keep_prefix();reentry_and_unrelated_source_writes();
    std::cout<<"source native listener configuration: 7 groups, 125 HRESULT triples, 8 late pointer cases, 24 blocked reentries PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
