#include "sgn/source_audio_sample_load.hpp"
#include <bit>
#include "sgn/source_audio_default_device_native.hpp"
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
    throw std::runtime_error("missing source-owner admission rejection at "+std::to_string(site));
}
struct Marker {};
enum class Output {zero,owned,unchanged};
struct Listener;struct SampleBuffer;struct Spatial;
enum class Region {full,larger,short_first,split,null_nonzero,null_zero,nonnull_zero,unwritten,ignored_bad_second,full_with_second};
struct BufferCall {std::uint32_t kind,offset,count,flags;std::uintptr_t receiver,first,second;std::uint32_t second_count;};
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
    HRESULT lock_hr=S_OK,unlock_hr=S_OK;int lock_throw{},unlock_throw{};
    Region region=Region::full;SampleBuffer* last_buffer{};
    std::vector<BufferCall> buffer_calls;
    std::function<void()> during_lock,during_unlock;
    unsigned lock_calls{},unlock_calls{};
    Stats(){events.reserve(1024);setter_calls.reserve(3);descriptors.reserve(128);buffer_calls.reserve(256);}
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
        ++s.query_calls;s.events.push_back("query");
        //Independent bytes from PE46FE80, not an expectation copied from the implementation constant.
        constexpr std::array<unsigned char,16> expected{0x84,0xfa,0x9a,0x27,0x81,0x49,0xce,0x11,0xa5,0x21,0x00,0x20,0xaf,0x0b,0xe5,0x60};
        require(out&&std::memcmp(&iid,expected.data(),16)==0,"QI IID/output address differs from PE");
        s.incoming_word=reinterpret_cast<std::uintptr_t>(*out);
        if(s.throw_before)throw Marker{};
        if(s.output==Output::zero)*out=nullptr;
        else if(s.output==Output::owned){auto* value=s.prepared_listener?s.prepared_listener:new Listener(s);value->AddRef();*out=value;}
        else if(s.query_hr==S_OK&&s.prepared_listener)s.prepared_listener->AddRef();
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
    Stats& s;ULONG references=0;Bytes storage,second_storage;
    explicit SampleBuffer(Stats& stats):s(stats),storage(s.descriptors.back().bytes+64,0xcd),second_storage(128,0xef){
        ++s.sample_live;s.sample_identity=reinterpret_cast<std::uintptr_t>(this);s.last_buffer=this;
    }
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
    HRESULT STDMETHODCALLTYPE Lock(DWORD offset,DWORD count,LPVOID* first,LPDWORD first_count,LPVOID* second,LPDWORD second_count,DWORD flags)override{
        require(first&&first_count&&second&&second_count&&first!=second&&first_count!=second_count,"source Lock requires four distinct output slots");
        ++s.lock_calls;s.events.push_back("lock");
        s.buffer_calls.push_back({11,offset,count,flags,reinterpret_cast<std::uintptr_t>(this),0,0,0});
        if(s.lock_throw==1)throw Marker{};
        require(count<4*1024*1024,"receiving fixture request too large");storage.assign(count+64,0xcd);second_storage.assign(count+64,0xef);
        if(s.region!=Region::unwritten){
            *first=storage.data()+16;*first_count=count;*second=nullptr;*second_count=0;
            if(s.region==Region::larger)*first_count=count+8;
            if(s.region==Region::short_first)*first_count=count?count-1:0;
            if(s.region==Region::split){*first_count=count/2;*second=second_storage.data()+16;*second_count=count-count/2;}
            if(s.region==Region::null_nonzero){*first=nullptr;*first_count=count?count:1;}
            if(s.region==Region::null_zero){*first=nullptr;*first_count=0;}
            if(s.region==Region::nonnull_zero)*first_count=0;
            if(s.region==Region::ignored_bad_second){*second=nullptr;*second_count=0xffffffffu;}
            if(s.region==Region::full_with_second){*second=second_storage.data()+16;*second_count=32;}
        }
        s.buffer_calls.back().first=reinterpret_cast<std::uintptr_t>(*first);s.buffer_calls.back().second=reinterpret_cast<std::uintptr_t>(*second);
        s.buffer_calls.back().second_count=*second_count;
        if(s.during_lock)s.during_lock();if(s.lock_throw==2)throw Marker{};return s.lock_hr;
    }
    HRESULT STDMETHODCALLTYPE Play(DWORD,DWORD,DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetVolume(LONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetPan(LONG)override{return no_call();}
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD)override{return no_call();}
    HRESULT STDMETHODCALLTYPE Stop()override{return no_call();}
    HRESULT STDMETHODCALLTYPE Unlock(LPVOID first,DWORD count,LPVOID second,DWORD second_count)override{
        ++s.unlock_calls;s.events.push_back("unlock");s.buffer_calls.push_back({19,0,count,0,reinterpret_cast<std::uintptr_t>(this),reinterpret_cast<std::uintptr_t>(first),reinterpret_cast<std::uintptr_t>(second),second_count});
        if(s.unlock_throw==1)throw Marker{};if(s.during_unlock)s.during_unlock();if(s.unlock_throw==2)throw Marker{};return s.unlock_hr;
    }
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
const SourceAudioDefaultDeviceNativeApi api{&create};
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
constexpr std::string_view resource="sound\\sfx\\petes-revving.wav";
std::uint32_t word(std::span<const std::uint8_t> b,std::size_t offset){std::uint32_t v{};std::memcpy(&v,b.data()+offset,4);return v;}
struct LoadRig:SampleRig {
    const AssetStore& assets;
    LoadRig(Stats& stats,const AssetStore& store,bool configure=true):SampleRig(stats,configure),assets(store){}
    SourceAudioSampleLoadResult load(std::uint32_t flags=0x20012,std::string_view name=resource){
        return std::get<SourceAudioSampleLoadResult>(load_source_audio_sample_after_listener(boundary,*owner,device,sample,fields,assets,name,enabled,flags));
    }
};
void sample_metadata(const LoadRig& r,unsigned flags,std::string_view name=resource){
    const auto wave=r.assets.resource(name);const auto bits=static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(word(wave,32)>>16)));
    const auto alignment=static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(word(wave,32))));
    require(r.sample.type==((flags&0x10)?2u:1u)&&r.sample.frequency==word(wave,24)&&r.fields.ready==1&&r.fields.bytes==word(wave,40)&&r.fields.bits==bits&&r.fields.channels==bits/(alignment<<3),"source metadata differs from WAV fields");
}
void copied(const Stats& stats,std::span<const std::uint8_t> payload,std::size_t count){
    const auto& memory=stats.last_buffer->storage;require(count<=payload.size()&&memory.size()>=count+32,"test copy extent invalid");
    require(std::equal(payload.begin(),payload.begin()+count,memory.begin()+16),"native first region differs from exact source payload");
    require(std::all_of(memory.begin(),memory.begin()+16,[](auto b){return b==0xcd;})&&std::all_of(memory.begin()+16+count,memory.end(),[](auto b){return b==0xcd;}),"copy touched guard/tail bytes");
    require(std::all_of(stats.last_buffer->second_storage.begin(),stats.last_buffer->second_storage.end(),[](auto b){return b==0xef;}),"copy used second region");
}
void not_copied(const Stats& stats){if(stats.last_buffer)require(std::all_of(stats.last_buffer->storage.begin(),stats.last_buffer->storage.end(),[](auto b){return b==0xcd;}),"failed prefix wrote payload");}
void native_trace(const Stats& stats,unsigned count){
    require(stats.buffer_calls.size()==2,"missing Lock/Unlock trace");const auto& a=stats.buffer_calls[0];const auto& b=stats.buffer_calls[1];
    require(a.kind==11&&a.offset==0&&a.count==count&&a.flags==2&&b.kind==19&&b.count==count&&a.receiver==b.receiver&&a.first==b.first&&b.second==0&&b.second_count==0,"Lock/Unlock retained receiver, pointer/count or source flags differ");
}
void hresult_matrix(const AssetStore& assets){
    constexpr HRESULT hrs[]{S_OK,S_FALSE,E_FAIL,E_INVALIDARG};const auto wave=assets.resource(resource);const auto payload=wave.subspan(44,word(wave,40));
    unsigned tested=0;
    for(bool spatial:{false,true})for(auto create_hr:hrs)for(auto qi:hrs)for(auto lock:hrs)for(auto unlock:hrs){
        if(!spatial&&qi!=0)continue;Stats stats;stats.create_hr=create_hr;stats.spatial_hr=qi;stats.lock_hr=lock;stats.unlock_hr=unlock;
        {LoadRig r(stats,assets);const unsigned flags=spatial?0x20012:2;const auto result=r.load(flags);
            const bool created=create_hr==0&&(!spatial||qi==0);const bool locked=created&&lock==0;
            require(result.source_return==(locked&&unlock==0)&&result.copy_phase_reached==locked,"source boolean/copy phase differs");
            require(result.buffer_retained&&result.spatial_query_attempted==(spatial&&create_hr==0)&&result.spatial_retained==(spatial&&create_hr==0),"retained output reporting differs");
            const auto last=!create_hr?(spatial&&qi!=0?0x461c19u:(lock!=0?0x461eecu:0x461f26u)):(spatial?0x461bfau:0x461cc8u);
            const auto hr=create_hr!=0?create_hr:(spatial&&qi!=0?qi:(lock!=0?lock:unlock));require(result.last_native_call==last&&result.last_native_result==hr,"native HRESULT/last-call diagnostic fabricated");
            require(stats.create_calls==1&&stats.spatial_calls==(spatial&&create_hr==0?1u:0u)&&stats.lock_calls==(created?1u:0u)&&stats.unlock_calls==(locked?1u:0u),"native prefix/order differs");
            require(r.sample.buffer.value==(created?stats.sample_identity:0)&&r.sample.spatial.value==(created&&spatial?stats.spatial_identity:0x22222222),"failed creator or2D source publication differs");sample_metadata(r,flags);
            if(locked){native_trace(stats,static_cast<unsigned>(payload.size()));copied(stats,payload,payload.size());}else not_copied(stats);
            not_released(stats);require(!r.owner->host_backend_ready(),"load constructed Modern");
        }released(stats);++tested;
    }require(tested==320,"HRESULT matrix case count differs");
}
void retail_waves(const AssetStore& assets){
    unsigned cases=0;
    for(const auto& definition:audio_sample_definitions)for(bool spatial:{false,true}){Stats stats;
        {LoadRig r(stats,assets,false);r.enabled=spatial?1u:0u;const auto result=r.load(0x20012,definition.resource);
            require(result.source_return&&result.buffer_retained&&result.copy_phase_reached&&result.spatial_query_attempted==spatial,"retail WAV load did not complete");
            sample_metadata(r,0x20012,definition.resource);const auto wave=assets.resource(definition.resource);const auto count=word(wave,40);
            native_trace(stats,count);copied(stats,wave.subspan(44,count),count);require(stats.descriptors[0].flags==(spatial?0x200f2u:0xe2u),"loader selected wrong source flags");
            not_released(stats);
        }released(stats,0);++cases;
    }require(cases==142,"retail single-sample test count differs");
}
void memory_regions(const AssetStore& assets){
    const auto wave=assets.resource(resource);const auto payload=wave.subspan(44,word(wave,40));
    for(auto region:{Region::larger,Region::ignored_bad_second,Region::full_with_second}){Stats stats;stats.region=region;
        {LoadRig r(stats,assets);const auto result=r.load();require(result.source_return,"valid first region rejected because of ignored outputs");native_trace(stats,static_cast<unsigned>(payload.size()));copied(stats,payload,payload.size());not_released(stats);}released(stats);}
    for(auto region:{Region::short_first,Region::split,Region::null_nonzero,Region::null_zero,Region::nonnull_zero,Region::unwritten}){Stats stats;stats.region=region;
        {LoadRig r(stats,assets);bool rejected=false;try{(void)r.load();}catch(const SourceAudioDefaultDeviceError& e){require(region==Region::null_nonzero&&e.source_site==0x461f0b,"wrong null-region boundary");rejected=true;}catch(const std::out_of_range&){require(region!=Region::null_nonzero,"null nonzero span formed before guard");rejected=true;}
            require(rejected&&stats.lock_calls==1&&stats.unlock_calls==0&&r.sample.buffer.value==stats.sample_identity,"unsupported memory performed copy/Unlock or rolled back sample");sample_metadata(r,0x20012);not_copied(stats);not_released(stats);
        }released(stats);
    }
    for(auto region:{Region::null_nonzero,Region::split,Region::unwritten}){Stats stats;stats.region=region;stats.lock_hr=S_FALSE;
        {LoadRig r(stats,assets);const auto result=r.load();require(!result.source_return&&result.last_native_result==S_FALSE&&!result.copy_phase_reached&&stats.unlock_calls==0,"failed Lock validated ignored outputs or compensated Unlock");not_copied(stats);not_released(stats);}released(stats);}
    for(auto region:{Region::null_zero,Region::nonnull_zero,Region::unwritten}){Stats stats;stats.region=region;
        {LoadRig r(stats,assets);stats.during_create=[&]{r.fields.bytes=0;};auto result=r.load();require(result.source_return&&result.copy_phase_reached&&r.fields.bytes==0,"zero-byte/null-first host domain rejected");native_trace(stats,0);copied(stats,payload,0);not_released(stats);}released(stats);}
    //Before Lock, a callback can corrupt the source count beyond the asset.
    Stats stats;{LoadRig r(stats,assets);stats.during_create=[&]{r.fields.bytes=static_cast<unsigned>(wave.size()-44+1);};
        bool rejected=false;try{(void)r.load();}catch(const std::out_of_range&){rejected=true;}
        require(rejected&&stats.lock_calls==1&&stats.unlock_calls==0,"shared asset extent guard changed the reached native prefix");not_copied(stats);not_released(stats);
    }released(stats);
}
void owner_mutations(const AssetStore& assets){
    const auto wave=assets.resource(resource);const auto payload=wave.subspan(44,word(wave,40));
    for(auto replacement:{std::uint64_t{0},std::uint64_t{0xdeadbeef}}){Stats stats;
        {LoadRig r(stats,assets);const auto original_device=r.device;stats.during_create=[&]{require(r.sample.buffer.value==0&&r.fields.ready==1,"metadata not visible during Create");r.device={123};r.enabled=77;};
            stats.during_lock=[&]{require(r.sample.buffer.value==stats.sample_identity,"creator publication missing at Lock");r.sample.buffer={replacement};r.sample.spatial={987};r.fields.bytes=0;r.fields.ready=33;};
            const auto result=r.load();require(result.source_return&&r.device.value==123&&r.enabled==77&&r.sample.buffer.value==replacement&&r.sample.spatial.value==987&&r.fields.bytes==0&&r.fields.ready==33,"source callback writes were rolled back");
            require(stats.descriptors[0].receiver==original_device.value,"creator changed device snapshot");native_trace(stats,static_cast<unsigned>(payload.size()));copied(stats,payload,payload.size());not_released(stats);
        }released(stats);
    }
    //Switch the source field to another retained same-owner buffer during Lock.
    Stats stats;{LoadRig r(stats,assets);require(r.run(false).native_interfaces_present,"old-buffer setup failed");const auto previous=r.sample.buffer;
        stats.during_lock=[&]{r.sample.buffer=previous;r.fields.bytes=1;};const auto result=r.load();
        require(result.source_return&&r.sample.buffer==previous&&stats.buffer_calls.back().receiver!=previous.value,"Unlock reread changed source buffer");
        native_trace(stats,static_cast<unsigned>(payload.size()));copied(stats,payload,payload.size());not_released(stats);
    }released(stats);
    //Unlock must ignore even a foreign owner's token written into the source field.
    Stats first,foreign;{LoadRig r(first,assets);LoadRig other(foreign,assets);require(other.run(false).native_interfaces_present,"foreign-buffer setup failed");const auto token=other.sample.buffer;
        first.during_lock=[&]{r.sample.buffer=token;};require(r.load().source_return&&r.sample.buffer==token&&first.buffer_calls.back().receiver!=token.value,"foreign source word redirected Unlock");copied(first,payload,payload.size());
    }released(first);released(foreign);
    for(auto bytes:{1u,2u,3u,5u,7u}){Stats short_copy;{LoadRig r(short_copy,assets);short_copy.during_create=[&]{r.fields.bytes=bytes;};require(r.load().source_return,"short retained copy failed");native_trace(short_copy,bytes);copied(short_copy,payload,bytes);}released(short_copy);}
}
void exceptions(const AssetStore& assets){
    const auto wave=assets.resource(resource);const auto payload=wave.subspan(44,word(wave,40));
    for(unsigned stage=0;stage<4;++stage)for(int point:{1,2}){Stats stats;
        {LoadRig r(stats,assets);auto mutate=[&]{r.sample.buffer={121};r.sample.spatial={232};r.fields.bytes=static_cast<unsigned>(payload.size());r.fields.ready=454;};
            if(stage==0){stats.create_throw=point;stats.during_create=mutate;}if(stage==1){stats.spatial_throw=point;stats.during_spatial=mutate;}
            if(stage==2){stats.lock_throw=point;stats.during_lock=mutate;}if(stage==3){stats.unlock_throw=point;stats.during_unlock=mutate;}
            bool caught=false;try{(void)r.load();}catch(const Marker&){caught=true;}require(caught,"native exception disappeared");
            require(r.sample.buffer.value==(point==2?121u:(stage<2?0:stats.sample_identity))&&r.sample.spatial.value==(point==2?232u:(stage<2?0x22222222:stats.spatial_identity))&&r.fields.ready==(point==2?454u:1u),"exception lost source prefix/callback writes");
            if(stage==3)copied(stats,payload,payload.size());else not_copied(stats);
            require(stats.unlock_calls==(stage==3?1u:0u),"exception performed compensating Unlock");not_released(stats);
            stats.create_throw=stats.spatial_throw=stats.lock_throw=stats.unlock_throw=0;stats.during_create=stats.during_spatial=stats.during_lock=stats.during_unlock={};
            require(r.load().source_return,"exception left operation guard armed");not_released(stats);
        }released(stats);
    }
}
struct Enumeration final:AudioEnumerationReceiver{bool visit(const std::optional<std::array<std::uint32_t,4>>&,std::string_view)override{throw std::runtime_error("reentry reached OS enumeration");}};
void reentry_and_threads(const AssetStore& assets){
    Stats stats;{LoadRig r(stats,assets);unsigned blocked=0;auto attempt=[&](auto&& f){try{f();}catch(const SourceAudioDefaultDeviceError&){++blocked;}};
        auto callback=[&]{AudioDeviceId d;AudioPrimaryId p;AudioBufferId b;Enumeration e;
            attempt([&]{(void)r.load();});attempt([&]{(void)r.run();});attempt([&]{(void)r.owner->create_source_default_device(d);});
            attempt([&]{(void)r.owner->set_source_cooperative_level(r.device,1,3);});attempt([&]{(void)r.owner->create_source_primary_buffer(r.device,{20,0x11,0,0,{}},p);});
            attempt([&]{(void)r.owner->complete_source_primary_format(r.device,r.primary,r.rate,r.capability,r.enabled);});attempt([&]{(void)r.acquire();});
            attempt([&]{(void)r.owner->configure_source_listener_after_query(r.listener);});attempt([&]{(void)r.owner->release_device(r.device);});
            attempt([&]{r.owner->prepare_host_backend();});attempt([&]{(void)r.owner->create_buffer(r.device,{},b);});attempt([&]{(void)r.owner->release(r.sample.buffer);});attempt([&]{(void)r.owner->enumerate(e);});
        };stats.during_create=stats.during_spatial=stats.during_lock=stats.during_unlock=callback;
        require(r.load().source_return&&blocked==52&&stats.create_calls==1&&stats.lock_calls==1&&stats.unlock_calls==1,"load allowed source mutation reentry");
        std::atomic<bool> refused=false;std::thread worker([&]{try{(void)r.load();}catch(const SourceAudioDefaultDeviceError&){refused=true;}});worker.join();require(refused&&stats.create_calls==1,"foreign thread entered load");not_released(stats);
    }released(stats);
    Stats cleanup;unsigned rejected=0;{LoadRig r(cleanup,assets);require(r.load().source_return,"cleanup fixture load failed");auto* owner=r.owner.get();cleanup.during_cleanup=[&]{try{(void)owner->load_source_sample(r.device,r.sample,r.fields,assets,resource,r.enabled,2);}catch(const SourceAudioDefaultDeviceError&){++rejected;}};
        r.owner.reset();cleanup.during_cleanup={};require(rejected==2,"native Release callback entered new load");
    }released(cleanup);
}
void missing_and_admission(const AssetStore& assets){
    Stats missing;{LoadRig r(missing,assets);const auto previous=r.sample;const auto fields=r.fields;r.device={123};
        const auto result=r.load(0x20012,"sound\\missing-a9.wav");require(!result.source_return&&!result.buffer_retained&&!result.last_native_call&&!result.last_native_result&&missing.create_calls==0,"missing asset entered native boundary or fabricated result");
        require(r.sample.buffer==previous.buffer&&r.sample.spatial==previous.spatial&&r.sample.type==previous.type&&r.sample.frequency==previous.frequency&&std::memcmp(&r.fields,&fields,sizeof fields)==0,"missing asset mutated metadata");
    }released(missing);
    for(bool spatial:{false,true})for(bool zero:{false,true}){Stats stats;{LoadRig r(stats,assets);r.device.value=zero?0:123;reject_at([&]{(void)r.load(spatial?0x20012:2);},spatial?0x461bef:0x461ca6);sample_metadata(r,spatial?0x20012:2);require(r.sample.buffer.value==0&&stats.create_calls==0,"invalid device rolled back source metadata");}released(stats);}
    for(bool spatial:{false,true})for(auto output:{Output::zero,Output::unchanged}){Stats stats;stats.buffer_output=output;
        {LoadRig r(stats,assets);reject_at([&]{(void)r.load(spatial?0x20012:2);},spatial?0x461c16:0x461ed7);require(stats.lock_calls==0&&r.sample.buffer.value==0,"missing native buffer became usable");not_released(stats);}released(stats);}
    Stats null_spatial;null_spatial.spatial_output=Output::zero;{LoadRig r(null_spatial,assets);const auto result=r.load();require(result.source_return&&result.buffer_retained&&!result.spatial_retained&&r.sample.spatial.value==0,"QIzero/null invented ready spatial interface or stopped source load");}released(null_spatial);
    Stats held;{Rig r(held);AudioSample sample;AudioSampleLoadFields fields;reject_at([&]{(void)load_source_audio_sample_after_listener(SourceAudioSamplesBoundary{},*r.owner,r.device,sample,fields,assets,resource,r.enabled,2);},0x461e00);require(held.create_calls==0,"forged boundary bypassed admission");}
    Stats failure;{LoadRig r(failure,assets);SourceAudioListenerContinuationOutcome previous=SourceAudioListenerAcquisitionFailure{0x461a97,S_FALSE};
        const auto value=load_source_audio_sample_after_listener(previous,*r.owner,r.device,r.sample,r.fields,assets,resource,r.enabled,2);require(std::get<SourceAudioListenerAcquisitionFailure>(value).query_result==S_FALSE&&failure.create_calls==0,"prior failure entered load");
    }released(failure);
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected retail asset root");AssetStore assets(argv[1]);
    hresult_matrix(assets);retail_waves(assets);memory_regions(assets);owner_mutations(assets);exceptions(assets);reentry_and_threads(assets);missing_and_admission(assets);
    std::cout<<"source native sample load:7 groups,320 HRESULT cases,142 retail WAV loads,8 exception prefixes,52 call reentries,2 cleanup reentries PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
