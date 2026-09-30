#include "sgn/source_audio_sample_creator.hpp"
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
struct Listener;struct SampleBuffer;struct Spatial;
struct Descriptor {DWORD size,flags,bytes,reserved;WAVEFORMATEX format;std::uintptr_t receiver;};
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
    HRESULT create_hr=S_OK,spatial_hr=S_OK;
    Output buffer_output=Output::owned,spatial_output=Output::owned;
    int create_throw{},spatial_throw{};
    unsigned sample_live{},spatial_live{},sample_releases{},spatial_releases{},sample_acquired{},spatial_acquired{};
    unsigned create_calls{},spatial_calls{},sample_adds{};
    std::uintptr_t sample_identity{},spatial_identity{};
    SampleBuffer* reused_buffer{};Spatial* reused_spatial{};
    std::function<void()> during_create,during_spatial,during_cleanup;
    std::vector<Descriptor> descriptors;
    Stats(){events.reserve(1024);setter_calls.reserve(3);descriptors.reserve(128);}
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
        ++s.primary_releases;s.events.push_back("primary_release");if(s.listener_live||s.sample_live||s.spatial_live||s.device_live!=1)++s.bad_order;
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
struct Spatial final:IDirectSound3DBuffer {
    Stats& s;ULONG references=0;
    explicit Spatial(Stats& stats):s(stats){++s.spatial_live;s.spatial_identity=reinterpret_cast<std::uintptr_t>(this);}
    ~Spatial(){--s.spatial_live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override{++s.unexpected;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{++s.sample_adds;return ++references;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(s.trap_foreign_release&&std::this_thread::get_id()!=s.owner_thread)std::_Exit(42);
        if(s.during_cleanup)s.during_cleanup();
        ++s.spatial_releases;s.events.push_back("spatial_release");if(!s.sample_live||s.device_live!=1||s.primary_live!=1)++s.bad_order;
        const auto left=--references;if(!left)delete this;return left;
    }
    HRESULT no_call(){++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetAllParameters(LPDS3DBUFFER)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetConeAngles(LPDWORD,LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetConeOrientation(D3DVECTOR*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetConeOutsideVolume(LPLONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetMaxDistance(D3DVALUE*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetMinDistance(D3DVALUE*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetMode(LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetPosition(D3DVECTOR*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetVelocity(D3DVECTOR*)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetAllParameters(LPCDS3DBUFFER,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetConeAngles(DWORD,DWORD,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetConeOrientation(D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetConeOutsideVolume(LONG,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetMaxDistance(D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetMinDistance(D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetMode(DWORD,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetPosition(D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetVelocity(D3DVALUE,D3DVALUE,D3DVALUE,DWORD)override{return no_call();}
};
struct SampleBuffer final:IDirectSoundBuffer {
    Stats& s;ULONG references=0;
    explicit SampleBuffer(Stats& stats):s(stats){++s.sample_live;s.sample_identity=reinterpret_cast<std::uintptr_t>(this);}
    ~SampleBuffer(){--s.sample_live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{
        ++s.spatial_calls;s.events.push_back("spatial_query");
        constexpr std::array<unsigned char,16> expected{0x86,0xfa,0x9a,0x27,0x81,0x49,0xce,0x11,0xa5,0x21,0x00,0x20,0xaf,0x0b,0xe5,0x60};
        require(out&&!*out&&std::memcmp(&iid,expected.data(),16)==0,"spatial QI IID/output transport differs");
        if(s.spatial_throw==1)throw Marker{};
        if(s.spatial_output==Output::zero)*out=nullptr;
        else if(s.spatial_output==Output::owned){auto* value=s.reused_spatial?s.reused_spatial:new Spatial(s);value->AddRef();++s.spatial_acquired;*out=value;}
        if(s.during_spatial)s.during_spatial();if(s.spatial_throw==2)throw Marker{};return s.spatial_hr;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{++s.sample_adds;return ++references;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(s.trap_foreign_release&&std::this_thread::get_id()!=s.owner_thread)std::_Exit(42);
        if(s.during_cleanup)s.during_cleanup();
        ++s.sample_releases;s.events.push_back("sample_release");if(s.spatial_live||s.primary_live!=1||s.device_live!=1)++s.bad_order;
        const auto remaining=--references;if(!remaining)delete this;return remaining;
    }
    HRESULT no_call(){++s.unexpected;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD,LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX,DWORD,LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetVolume(LPLONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetPan(LPLONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND,LPCDSBUFFERDESC)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Lock(DWORD,DWORD,LPVOID*,LPDWORD,LPVOID*,LPDWORD,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Play(DWORD,DWORD,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX)override{return no_call();}
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
        ++s.device_releases;s.events.push_back("device_release");if(s.primary_live||s.listener_live||s.sample_live||s.spatial_live)++s.bad_order;
        const auto remaining=--references;if(!remaining)delete this;return remaining;
    }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC d,LPDIRECTSOUNDBUFFER* out,LPUNKNOWN outer)override{
        if(d&&d->lpwfxFormat){
            require(out&&!*out&&!outer,"sample create output/aggregation differs");
            require(d->dwSize==sizeof(DSBUFFERDESC1)&&d->dwSize==24&&d->dwReserved==0,"native descriptor prefix differs");
            ++s.create_calls;s.events.push_back("sample_create");
            s.descriptors.push_back({d->dwSize,d->dwFlags,d->dwBufferBytes,d->dwReserved,*d->lpwfxFormat,reinterpret_cast<std::uintptr_t>(this)});
            if(s.create_throw==1)throw Marker{};
            if(s.buffer_output==Output::zero)*out=nullptr;
            else if(s.buffer_output==Output::owned){auto* value=s.reused_buffer?s.reused_buffer:new SampleBuffer(s);value->AddRef();++s.sample_acquired;*out=value;}
            if(s.during_create)s.during_create();if(s.create_throw==2)throw Marker{};return s.create_hr;
        }
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

struct SampleRig:Rig {
    AudioSample sample{{0x11111111},{0x22222222},7,22050};
    AudioSampleLoadFields fields{9,65870,16,2};
    SourceAudioListenerContinuationOutcome boundary{SourceAudioSamplesBoundary{}};
    explicit SampleRig(Stats& stats,bool configure=true):Rig(stats){
        prepare();if(!configure)mode=0;
        boundary=continue_source_audio_listener_461aa0(acquire(),*owner);
        require(std::holds_alternative<SourceAudioSamplesBoundary>(boundary),"sample setup did not reach A7/skip");
    }
    SourceAudioSampleCreationResult run(bool spatial=true,std::uint32_t flags=0x20012){
        return std::get<SourceAudioSampleCreationResult>(create_source_audio_sample_after_listener(boundary,*owner,device,sample,fields,spatial,flags));
    }
};
void released(const Stats& s,unsigned listeners=1){
    require(s.sample_releases==s.sample_acquired&&s.spatial_releases==s.spatial_acquired&&s.sample_adds==s.sample_acquired+s.spatial_acquired,
        "sample COM reference loss, extra AddRef or premature release");
    require(s.listener_releases==listeners&&s.primary_releases==1&&s.device_releases==1&&s.adds==listeners,
        "retained parent lifetime changed");
    require(!s.sample_live&&!s.spatial_live&&!s.listener_live&&!s.primary_live&&!s.device_live&&!s.bad_order&&!s.unexpected,
        "live COM reference, wrong cleanup order or extra native call");
}
void not_released(const Stats& s){require(!s.sample_releases&&!s.spatial_releases&&!s.listener_releases&&!s.primary_releases&&!s.device_releases,"native output released before final same-owner cleanup");}
void metadata(const SampleRig& r){require(r.sample.type==7&&r.sample.frequency==22050&&r.fields.ready==9&&r.fields.bytes==65870&&r.fields.bits==16&&r.fields.channels==2,"creator changed loader metadata");}
void descriptor(const Descriptor& d,unsigned flags,unsigned bytes,unsigned channels,unsigned rate,unsigned average,unsigned align,unsigned bits){
    require(d.size==24&&d.flags==flags&&d.bytes==bytes&&d.reserved==0&&d.format.wFormatTag==1&&d.format.nChannels==channels&&
        d.format.nSamplesPerSec==rate&&d.format.nAvgBytesPerSec==average&&d.format.nBlockAlign==align&&d.format.wBitsPerSample==bits&&d.format.cbSize==0,
        "source PCM fields/flags or native descriptor transport differ");
}
constexpr std::array<HRESULT,5> results{S_OK,S_FALSE,E_FAIL,E_INVALIDARG,static_cast<HRESULT>(0x80000000u)};
constexpr std::array<Output,3> outputs{Output::zero,Output::owned,Output::unchanged};
void create_matrix(){
    for(bool spatial:{false,true})for(auto hr:results)for(auto output:outputs){Stats s;s.create_hr=hr;s.buffer_output=output;
        {SampleRig r(s);const bool owned=output==Output::owned;
            if(spatial&&hr==0&&!owned)reject_at([&]{(void)r.run();},0x461c16);
            else{const auto value=r.run(spatial);require(value.source_return==(hr==0)&&value.native_interfaces_present==(hr==0&&owned)&&value.source_function==(spatial?0x461b70u:0x461c40u),"create HRESULT/diagnostic differs");
                require(r.sample.buffer.value==(hr==0?(owned?s.sample_identity:0):0x11111111),"create source publication differs");
                require(r.sample.spatial.value==(hr==0&&spatial?s.spatial_identity:0x22222222),"2D/failed create overwrote spatial field");}
            require(s.create_calls==1&&s.spatial_calls==(spatial&&hr==0&&owned?1u:0u),"creation continuation count differs");
            descriptor(s.descriptors[0],spatial?0x200f2:0xe2,65870,2,22050,88200,4,16);metadata(r);not_released(s);
            require(!r.owner->host_backend_ready(),"creator prepared Modern");
        }released(s);
    }
}
void query_matrix(){
    for(auto hr:results)for(auto output:outputs){Stats s;s.spatial_hr=hr;s.spatial_output=output;
        {SampleRig r(s);auto value=r.run();require(value.source_return==(hr==0)&&value.native_interfaces_present==(hr==0&&output==Output::owned),"QI zero/nonzero or null diagnostic differs");
            require(r.sample.buffer.value==(hr==0?s.sample_identity:0x11111111)&&r.sample.spatial.value==(hr==0?(output==Output::owned?s.spatial_identity:0):0x22222222),"QI source publication differs");
            require(s.create_calls==1&&s.spatial_calls==1,"QI count differs");metadata(r);not_released(s);
        }released(s);
    }
}
void format_and_flags(){
    //Independent arithmetic fixtures: high bits exercise DWORD product then WORD truncation.
    struct Case{unsigned bits,channels,rate,bytes,block,average;};
    constexpr Case cases[]{{8,1,11025,1,1,11025},{16,2,48000,1024,4,192000},{24,6,96000,4096,18,1728000},
        {0xfffffff0u,0x10002u,0xffffffefu,0xffffffffu,65532,0xffef0044u},{0,0,0,0,0,0}};
    for(bool spatial:{false,true})for(unsigned flags:{0u,2u,0x10u,0x20012u,0xffffffffu})for(auto c:cases){Stats s;
        {SampleRig r(s,false);r.fields={77,c.bytes,c.bits,c.channels};r.sample.frequency=c.rate;const auto v=r.run(spatial,flags);
            require(v.source_return&&v.native_interfaces_present&&s.query_calls==0,"source A6 skip lost creator admission");
            descriptor(s.descriptors[0],spatial?(flags|0xe0):0xe2,c.bytes,c.channels&0xffff,c.rate,c.average,c.block,c.bits&0xffff);
            require(r.fields.ready==77&&s.descriptors[0].receiver==r.device.value,"creator changed ready or device receiver");not_released(s);
        }released(s,0);
    }
}
void exception_prefixes(){
    for(bool query:{false,true})for(int point:{1,2})for(auto output:outputs){Stats s;
        if(query){s.spatial_throw=point;s.spatial_output=output;}else{s.create_throw=point;s.buffer_output=output;}
        {SampleRig r(s);auto mutation=[&]{r.sample.buffer={333};r.sample.spatial={444};r.fields.ready=555;};
            if(query)s.during_spatial=mutation;else s.during_create=mutation;
            bool caught=false;try{(void)r.run();}catch(const Marker&){caught=true;}
            require(caught&&r.sample.buffer.value==(point==2?333u:0x11111111u)&&r.sample.spatial.value==(point==2?444u:0x22222222u)&&r.fields.ready==(point==2?555u:9u),"exception lost reached callback writes or prematurely published local outputs");
            require(s.sample_acquired==(query||point==2&&output==Output::owned?1u:0u)&&s.spatial_acquired==(query&&point==2&&output==Output::owned?1u:0u),"exception output ownership prefix differs");
            r.fields.ready=9;metadata(r);not_released(s);s.during_create=s.during_spatial={};
            //Creator failure is not global initialization failure; later calls reuse the same device.
            s.create_throw=s.spatial_throw=0;s.buffer_output=s.spatial_output=Output::owned;require(r.run().native_interfaces_present,"activity guard remained armed after exception");
        }released(s);
    }
}
void mutation_and_retention(){
    for(bool fail_query:{false,true}){Stats s;s.spatial_hr=fail_query?S_FALSE:S_OK;
        {SampleRig r(s);const auto original_device=r.device.value;
            s.during_create=[&]{require(r.sample.buffer.value==0x11111111&&r.sample.spatial.value==0x22222222,"sample published during Create");
                r.sample.buffer={101};r.sample.spatial={202};r.sample.type=303;r.sample.frequency=404;r.fields={505,606,707,808};r.device={999};};
            s.during_spatial=[&]{require(r.sample.buffer.value==101&&r.sample.spatial.value==202,"buffer published before successful QI return");r.sample.buffer={909};r.sample.spatial={1001};};
            const auto v=r.run();require(v.source_return==!fail_query,"QI mutation return differs");
            require(r.sample.buffer.value==(fail_query?909:s.sample_identity)&&r.sample.spatial.value==(fail_query?1001:s.spatial_identity),"source publication/earlier callback writes lost");
            require(r.device.value==999&&s.descriptors[0].receiver==original_device&&r.sample.type==303&&r.sample.frequency==404&&r.fields.ready==505&&r.fields.bytes==606,"late source mutation reverted");
            descriptor(s.descriptors[0],0x200f2,65870,2,22050,88200,4,16);not_released(s);
        }released(s);
    }
    //Same object returned twice supplies two acquired references, not one deduplicated identity.
    Stats s;{SampleRig r(s);s.reused_buffer=new SampleBuffer(s);s.reused_spatial=new Spatial(s);
        require(r.run().native_interfaces_present,"first shared output failed");const auto b=r.sample.buffer;const auto p=r.sample.spatial;
        require(r.run().native_interfaces_present&&r.sample.buffer==b&&r.sample.spatial==p&&s.sample_acquired==2&&s.spatial_acquired==2,"same-valued acquired outputs deduplicated");not_released(s);
    }released(s);
    //Overwriting a previously published source record keeps both sets of references.
    Stats many;{SampleRig r(many);for(unsigned i=0;i<16;++i){require(r.run(i%2==0).native_interfaces_present,"repeated creator failed");not_released(many);}
        require(many.sample_acquired==16&&many.spatial_acquired==8,"registry replaced previous acquisitions");
    }released(many);
}
struct Enumeration final:AudioEnumerationReceiver{bool visit(const std::optional<std::array<std::uint32_t,4>>&,std::string_view)override{throw std::runtime_error("reentry reached native enumeration");}};
void reentry_and_threads(){
    Stats s;{SampleRig r(s);unsigned blocked=0;
        auto attempt=[&](auto&& f){try{f();}catch(const SourceAudioDefaultDeviceError&){++blocked;}};
        auto callback=[&]{AudioDeviceId d;AudioPrimaryId p;AudioBufferId b;Enumeration e;
            attempt([&]{(void)r.run();});attempt([&]{(void)r.owner->create_source_default_device(d);});
            attempt([&]{(void)r.owner->set_source_cooperative_level(r.device,1,3);});
            attempt([&]{(void)r.owner->create_source_primary_buffer(r.device,{20,0x11,0,0,{}},p);});
            attempt([&]{(void)r.owner->complete_source_primary_format(r.device,r.primary,r.rate,r.capability,r.enabled);});
            attempt([&]{(void)r.acquire();});attempt([&]{(void)r.owner->configure_source_listener_after_query(r.listener);});
            attempt([&]{(void)r.owner->release_device(r.device);});attempt([&]{r.owner->prepare_host_backend();});
            attempt([&]{(void)r.owner->create_buffer(r.device,{},b);});attempt([&]{(void)r.owner->release(r.sample.buffer);});
            attempt([&]{(void)r.owner->enumerate(e);});
        };
        s.during_create=s.during_spatial=callback;require(r.run().native_interfaces_present&&blocked==24,"reentrant native/modern mutation escaped guard");
        std::atomic<bool> refused=false;std::thread worker([&]{try{(void)r.run();}catch(const SourceAudioDefaultDeviceError&){refused=true;}});worker.join();
        require(refused&&s.create_calls==1&&s.spatial_calls==1,"foreign thread reached sample creation");not_released(s);
    }released(s);
}
void cleanup_reentry(){
    Stats s;unsigned blocked=0;
    {SampleRig r(s);require(r.run().native_interfaces_present,"cleanup setup failed");auto* owner=r.owner.get();
        s.during_cleanup=[&]{
            try{(void)owner->create_source_sample_buffer(r.device,r.sample,r.fields,false,2);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
            Enumeration e;try{(void)owner->enumerate(e);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        };
        r.owner.reset();s.during_cleanup={};require(blocked==4&&s.create_calls==1,"cleanup Release allowed new source operation");
    }released(s);
}
void admission(){
    for(auto enabled:{0u,2u,0xffffffffu}){Stats s;{Rig r(s);r.prepare();r.enabled=enabled;
        const auto boundary=continue_source_audio_listener_461aa0(r.acquire(),*r.owner);
        AudioSample sample{{1},{2},3,44100};AudioSampleLoadFields fields{4,1024,16,2};
        const auto value=create_source_audio_sample_after_listener(boundary,*r.owner,r.device,sample,fields,false,2);
        require(std::get<SourceAudioSampleCreationResult>(value).native_interfaces_present&&r.enabled==enabled&&s.query_calls==0,"enabled!=1 skip lost creator admission or mutated source");
    }released(s,0);}
    for(auto throw_at:{0,1,2}){Stats s;{Rig r(s);r.prepare();const auto acquired=r.acquire();s.throwing_setter=throw_at;
        try{(void)continue_source_audio_listener_461aa0(acquired,*r.owner);throw std::runtime_error("setter exception missing");}catch(const Marker&){}
        AudioSample sample;AudioSampleLoadFields fields;
        reject_at([&]{(void)r.owner->create_source_sample_buffer(r.device,sample,fields,true,2);},0x461b70);
        require(s.create_calls==0,"throwing A7 granted sample admission");
    }released(s);}

    for(unsigned stage:{0u,1u,2u,3u}){Stats s;{Rig r(s);AudioSample sample{{123},{456},0,44100};AudioSampleLoadFields fields{0,1024,16,2};
        if(stage)r.prepare();if(stage==2){s.query_hr=S_FALSE;(void)r.acquire();}if(stage==3)(void)r.acquire();
        reject_at([&]{(void)create_source_audio_sample_after_listener(SourceAudioSamplesBoundary{},*r.owner,r.device,sample,fields,true,2);},0x461b70);
        require(sample.buffer.value==123&&sample.spatial.value==456&&s.create_calls==0,"forged boundary bypassed actual host history");
        if(stage==3){r.owner->configure_source_listener_after_query(r.listener);require(r.owner->create_source_sample_buffer(r.device,sample,fields,false,2).native_interfaces_present,"real A7 continuation failed");}
    }if(stage)released(s,stage>=2?1u:0u);else require(!s.device_live&&!s.unexpected,"deferred-only admission called native");}
    for(bool zero:{false,true}){Stats s;{SampleRig r(s);r.device.value=zero?0:123;
        reject_at([&]{(void)r.run();},0x461bef);reject_at([&]{(void)r.run(false);},0x461ca6);
        require(s.create_calls==0&&r.sample.buffer.value==0x11111111,"bad source device entered COM");
    }released(s);}
    Stats s;{SampleRig r(s);const SourceAudioListenerContinuationOutcome failure=SourceAudioListenerAcquisitionFailure{0x461a97,S_FALSE};
        const auto v=create_source_audio_sample_after_listener(failure,*r.owner,r.device,r.sample,r.fields,true,2);
        require(std::get<SourceAudioListenerAcquisitionFailure>(v).query_result==S_FALSE&&s.create_calls==0,"failed prior stage created sample");
    }released(s);
}
struct DeathContext {bool armed{};const WindowsAudioDevice* expected{};const WindowsAudioDevice* deleting{};const Stats* stats{};std::uintptr_t buffer{},spatial{};};
thread_local DeathContext death;
[[noreturn]]void fail_terminate()noexcept{std::_Exit(41);}
[[noreturn]]void sample_terminate()noexcept{
    const auto& c=death;const auto* s=c.stats;
    if(c.armed&&c.expected&&c.expected==c.deleting&&s&&s->trap_foreign_release&&std::this_thread::get_id()!=s->owner_thread&&
        c.buffer&&c.spatial&&c.buffer==s->sample_identity&&c.spatial==s->spatial_identity&&s->device_live==1&&s->primary_live==1&&s->listener_live==1&&
        s->sample_live==1&&s->spatial_live==1&&s->sample_acquired==1&&s->spatial_acquired==1&&!s->sample_releases&&!s->spatial_releases&&
        !s->device_releases&&!s->primary_releases&&!s->listener_releases)std::_Exit(0);
    std::_Exit(41);
}
int foreign_destroy(std::string_view mode){
    std::set_terminate(fail_terminate);if(mode=="--death-before-setup")std::terminate();
    Stats s;s.trap_foreign_release=true;SampleRig r(s);require(r.run().native_interfaces_present,"death fixture not acquired");
    const auto* expected=r.owner.get();std::set_terminate(sample_terminate);if(mode=="--death-main")std::terminate();auto* raw=r.owner.release();
    std::thread worker([&,raw]{
        std::set_terminate(sample_terminate);
        if(mode=="--death-unarmed-worker")std::terminate();death={true,expected,raw,&s,r.sample.buffer.value,r.sample.spatial.value};
        if(mode=="--death-wrong-buffer"){++death.buffer;std::terminate();}
        if(mode=="--death-wrong-spatial"){++death.spatial;std::terminate();}
        if(mode=="--death-wrong-object"){death.deleting=nullptr;std::terminate();}
        if(mode=="--death-other-worker"){std::thread other([]{std::set_terminate(sample_terminate);std::terminate();});other.join();}
        if(mode=="--death-native-release"){reinterpret_cast<IDirectSoundBuffer*>(r.sample.buffer.value)->Release();std::_Exit(1);}
        delete raw;std::_Exit(1);
    });worker.join();return 1;
}
}
int main(int argc,char** argv){try{
    if(argc==2){const std::string_view mode=argv[1];
        if(mode=="--foreign-destroy-sample"||mode=="--death-before-setup"||mode=="--death-main"||mode=="--death-unarmed-worker"||
            mode=="--death-wrong-buffer"||mode=="--death-wrong-spatial"||mode=="--death-wrong-object"||mode=="--death-other-worker"||mode=="--death-native-release")return foreign_destroy(mode);
        return 2;
    }if(argc!=1)return 2;
    create_matrix();query_matrix();format_and_flags();exception_prefixes();mutation_and_retention();reentry_and_threads();cleanup_reentry();admission();
    std::cout<<"source native sample creator: 8 groups,30 Create cases,15 QI cases,50 descriptor cases,12 exception cases,24 blocked reentries,4 cleanup reentries PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
