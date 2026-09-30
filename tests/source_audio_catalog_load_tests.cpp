#include "sgn/source_audio_catalog_load.hpp"
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
    std::function<void(unsigned)> before_native;
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
        if(s.before_native)s.before_native(1);
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
        if(s.before_native)s.before_native(2);
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
        if(s.before_native)s.before_native(3);
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
            if(s.before_native)s.before_native(0);
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
void copied(const Stats& stats,std::span<const std::uint8_t> payload,std::size_t count){
    const auto& memory=stats.last_buffer->storage;require(count<=payload.size()&&memory.size()>=count+32,"test copy extent invalid");
    require(std::equal(payload.begin(),payload.begin()+count,memory.begin()+16),"native first region differs from exact source payload");
    require(std::all_of(memory.begin(),memory.begin()+16,[](auto b){return b==0xcd;})&&std::all_of(memory.begin()+16+count,memory.end(),[](auto b){return b==0xcd;}),"copy touched guard/tail bytes");
    require(std::all_of(stats.last_buffer->second_storage.begin(),stats.last_buffer->second_storage.end(),[](auto b){return b==0xef;}),"copy used second region");
}
void not_copied(const Stats& stats){if(stats.last_buffer)require(std::all_of(stats.last_buffer->storage.begin(),stats.last_buffer->storage.end(),[](auto b){return b==0xcd;}),"failed prefix wrote payload");}

//A10 reuses the sealed A9 SDK fake transport above. Expected loop order and
//field observations below are independent receiving assertions for461F50.
struct CatalogRig:SampleRig {
    const AssetStore& assets;
    AudioDriverState driver;
    AudioSampleLoadStorage storage;
    std::array<std::uint32_t,72> types{};
    CatalogRig(Stats& stats,const AssetStore& store):SampleRig(stats),assets(store){
        for(const auto& d:audio_sample_definitions)types[d.slot]=d.type;
        types[0]=0xabcd;
        driver.initialized=17;driver.spatial_mode=23;driver.range=29;driver.range_squared=31;
        driver.inverse_range=37;driver.duplicate_count=41;driver.playing_count=43;
        driver.spatial_template=audio_default_spatial_parameters();
        for(auto& slot:driver.samples)slot={{0x1111111122222222ull},{0x3333333344444444ull},47,53};
        for(auto& field:storage.samples)field={59,61,67,71};
    }
    SourceAudioCatalogLoadResult catalog(){
        return std::get<SourceAudioCatalogLoadResult>(load_source_audio_catalog_after_listener(
            boundary,*owner,driver,storage,types,device,assets,enabled));
    }
    SourceAudioSampleLoadResult single(){
        return std::get<SourceAudioSampleLoadResult>(load_source_audio_sample_after_listener(
            boundary,*owner,device,sample,fields,assets,resource,enabled,0x20012));
    }
    void selected(bool spatial){types.fill(0);types[1]=types[2]=types[3]=spatial?2u:1u;enabled=1;}
};
void zero_slot(const CatalogRig& r,unsigned slot){
    const auto& v=r.driver.samples[slot];const auto& f=r.storage.samples[slot];
    require(v.buffer.value==0&&v.spatial.value==0&&v.type==0&&v.frequency==0&&f.ready==0&&f.bytes==0&&f.bits==0&&f.channels==0,"source sample reset or unvisited suffix differs");
}
void parents_unchanged(const CatalogRig& r){
    require(r.driver.initialized==17&&r.driver.spatial_mode==23&&r.driver.range==29&&r.driver.range_squared==31&&r.driver.inverse_range==37&&r.driver.duplicate_count==41&&r.driver.playing_count==43,"catalog changed driver state outside sample reset");
    const auto spatial=audio_default_spatial_parameters();
    require(std::memcmp(&r.driver.spatial_template,&spatial,sizeof spatial)==0,"catalog changed spatial template");
    require(!r.owner->host_backend_ready(),"catalog constructed Modern");
}
void metadata(const CatalogRig& r,unsigned slot,unsigned type){
    const auto wave=r.assets.resource(audio_sample_definitions[slot-1].resource);
    const auto& sample=r.driver.samples[slot];const auto& f=r.storage.samples[slot];
    const auto bits=static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(word(wave,32)>>16)));
    const auto align=static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(word(wave,32))));
    require(sample.type==type&&sample.frequency==word(wave,24)&&f.ready==1&&f.bytes==word(wave,40)&&f.bits==bits&&f.channels==bits/(align<<3),"catalog metadata differs from source WAV44");
}
void check_trace(const Stats& s){
    require(s.buffer_calls.size()==2*s.unlock_calls,"complete trace missing a Lock/Unlock pair");
    for(std::size_t i=0;i<s.buffer_calls.size();i+=2){const auto& a=s.buffer_calls[i];const auto& b=s.buffer_calls[i+1];
        require(a.kind==11&&a.offset==0&&a.flags==2&&b.kind==19&&a.count==b.count&&a.receiver==b.receiver&&a.first==b.first&&b.second==0&&b.second_count==0,"catalog Lock/Unlock identity/count/pointer changed");
    }
}
void full_catalogs(const AssetStore& assets){
    for(auto mode:{0u,1u,2u,0xffffffffu}){Stats stats;{CatalogRig r(stats,assets);r.enabled=mode;
        std::vector<unsigned> order;
        if(mode==0){for(const auto& d:audio_sample_definitions)order.push_back(d.slot);}
        else{for(auto kind:{2u,1u})for(const auto& d:audio_sample_definitions)if(d.type==kind)order.push_back(d.slot);}
        unsigned spatial_count=0;
        stats.during_create=[&]{const auto index=stats.create_calls-1;const auto slot=order[index];
            if(index==0){for(unsigned i=0;i<128;++i)if(i!=slot)zero_slot(r,i);}
            const unsigned type=mode==0?1u:audio_sample_definitions[slot-1].type;metadata(r,slot,type);
            require(r.driver.samples[slot].buffer.value==0,"source sample buffer published before Create return");
            const bool spatial=mode==1&&type==2;spatial_count+=spatial?1u:0u;
            const auto& d=stats.descriptors.back();const auto wave=assets.resource(audio_sample_definitions[slot-1].resource);
            require(d.flags==(spatial?0x200f2u:0xe2u)&&d.bytes==word(wave,40)&&d.format.nSamplesPerSec==word(wave,24)&&d.receiver==r.device.value,"catalog descriptor/order/device differs");
            not_released(stats);parents_unchanged(r);
        };
        stats.during_unlock=[&]{const auto slot=order[stats.create_calls-1];const auto wave=assets.resource(audio_sample_definitions[slot-1].resource);copied(stats,wave.subspan(44),word(wave,40));};
        const auto result=r.catalog();require(result.source_return&&result.last_native_call==0x461f26&&result.last_native_result==0,"complete catalog result differs");
        require(stats.create_calls==71&&stats.spatial_calls==spatial_count&&stats.lock_calls==71&&stats.unlock_calls==71,"catalog omitted/repeated sample load");
        for(const auto& d:audio_sample_definitions){const auto& v=r.driver.samples[d.slot];require(v.buffer.value!=0&&(v.spatial.value!=0)==(mode==1&&d.type==2),"catalog native identities missing or fabricated");metadata(r,d.slot,mode==0?1u:d.type);require(r.types[d.slot]==(mode==0?1u:d.type),"descriptor type overwritten without source store");}
        require(r.types[0]==0xabcd,"descriptor zero changed");zero_slot(r,0);for(unsigned i=72;i<128;++i)zero_slot(r,i);
        parents_unchanged(r);check_trace(stats);not_released(stats);
    }released(stats);}
}
void hresult_matrix(const AssetStore& assets){
    constexpr HRESULT hrs[]{S_OK,S_FALSE,E_FAIL,E_INVALIDARG};unsigned cases=0;
    for(bool spatial:{false,true})for(auto create_hr:hrs)for(auto qi:hrs)for(auto lock:hrs)for(auto unlock:hrs){
        if(!spatial&&qi!=0)continue;Stats stats;stats.create_hr=create_hr;stats.spatial_hr=qi;stats.lock_hr=lock;stats.unlock_hr=unlock;
        {CatalogRig r(stats,assets);r.selected(spatial);const auto result=r.catalog();
            const bool created=create_hr==0&&(!spatial||qi==0);const bool locked=created&&lock==0;
            require(result.source_return,"461F50 propagated an ignored single-load false");
            require(stats.create_calls==3&&stats.spatial_calls==(spatial&&create_hr==0?3u:0u)&&stats.lock_calls==(created?3u:0u)&&stats.unlock_calls==(locked?3u:0u),"HRESULT terminated catalog or retained per-call Lock state");
            const auto last=create_hr!=0?(spatial?0x461bfau:0x461cc8u):(spatial&&qi!=0?0x461c19u:(lock!=0?0x461eecu:0x461f26u));
            const auto hr=create_hr!=0?create_hr:(spatial&&qi!=0?qi:(lock!=0?lock:unlock));require(result.last_native_call==last&&result.last_native_result==hr,"last reached native call/result differs");
            for(unsigned slot=1;slot<=3;++slot){metadata(r,slot,spatial?2u:1u);const auto& v=r.driver.samples[slot];require((v.buffer.value!=0)==created&&(v.spatial.value!=0)==(created&&spatial),"failure prefix published incorrect sample handles");}
            for(unsigned slot=4;slot<128;++slot)zero_slot(r,slot);zero_slot(r,0);parents_unchanged(r);not_released(stats);
            if(locked){const auto wave=assets.resource(audio_sample_definitions[2].resource);copied(stats,wave.subspan(44),word(wave,40));check_trace(stats);}else not_copied(stats);
        }released(stats);++cases;
    }require(cases==320,"catalog HRESULT matrix count differs");
    //A failure in the middle does not suppress the last real load.
    for(unsigned stage=0;stage<4;++stage){Stats stats;{CatalogRig r(stats,assets);r.selected(true);
        stats.before_native=[&](unsigned){const bool fail=stats.create_calls==2;stats.create_hr=stage==0&&fail?S_FALSE:S_OK;stats.spatial_hr=stage==1&&fail?E_FAIL:S_OK;stats.lock_hr=stage==2&&fail?E_INVALIDARG:S_OK;stats.unlock_hr=stage==3&&fail?S_FALSE:S_OK;};
        const auto result=r.catalog();require(result.source_return&&result.last_native_result==0&&r.driver.samples[3].buffer.value!=0&&r.storage.samples[2].ready==1&&stats.create_calls==3,"middle failed sample stopped catalog or rolled back ready");
        require((r.driver.samples[2].buffer.value==0)==(stage<2),"middle failure source publication differs");not_released(stats);
    }released(stats);}
}
void mutations(const AssetStore& assets){
    //461F86 rereads enabled in the first loop;461FE2 rereads types in the second.
    for(unsigned stage=0;stage<4;++stage){Stats stats;{CatalogRig r(stats,assets);r.types.fill(0);r.types[1]=2;r.types[2]=2;r.types[3]=2;
        auto mutate=[&]{if(stats.create_calls==1){r.enabled=0;r.types[1]=1;}};
        if(stage==0)stats.during_create=mutate;if(stage==1)stats.during_spatial=mutate;if(stage==2)stats.during_lock=mutate;if(stage==3)stats.during_unlock=mutate;
        require(r.catalog().source_return&&stats.create_calls==72&&stats.spatial_calls==1&&stats.lock_calls==72&&stats.unlock_calls==72,"mode/type late writes did not affect both source loops");
        for(unsigned slot=1;slot<=71;++slot){metadata(r,slot,1);require(r.types[slot]==1&&r.driver.samples[slot].buffer.value!=0,"late mode zero did not rewrite remaining descriptors");}
        //The second2D load writes only buffer; retained first3D spatial stays.
        require(r.driver.samples[1].spatial.value!=0&&r.enabled==0,"second pass cleared source spatial or mode write");
        not_released(stats);parents_unchanged(r);
    }released(stats);}
    {Stats stats;{CatalogRig r(stats,assets);r.types.fill(0);r.types[1]=2;r.types[2]=2;r.types[3]=2;
        stats.during_create=[&]{if(stats.create_calls==1){r.types[1]=1;r.types[2]=77;r.types[3]=1;r.types[71]=1;}else if(stats.create_calls==2){r.types[2]=1;r.types[3]=77;}};
        require(r.catalog().source_return&&stats.create_calls==4&&stats.spatial_calls==1,"descriptor changes were snapshotted or source pass order changed");
        metadata(r,1,1);metadata(r,2,1);zero_slot(r,3);metadata(r,71,1);require(r.driver.samples[1].spatial.value!=0,"2D overwrite cleared previous3D interface");not_released(stats);
    }released(stats);}
    //After the first native call, the next creator must see actual changed device.
    for(unsigned stage=0;stage<4;++stage)for(auto invalid:{0ull,0x12345678abcdef01ull}){Stats stats;{CatalogRig r(stats,assets);r.selected(true);const auto valid=r.device;
        auto mutate=[&]{if(stats.create_calls==1)r.device={invalid};};
        if(stage==0)stats.during_create=mutate;if(stage==1)stats.during_spatial=mutate;if(stage==2)stats.during_lock=mutate;if(stage==3)stats.during_unlock=mutate;
        reject_at([&]{(void)r.catalog();},0x461bef);require(stats.create_calls==1&&stats.unlock_calls==1&&r.driver.samples[1].buffer.value!=0&&r.driver.samples[2].buffer.value==0&&r.device.value==invalid,"late device read copied owner state or lost reached prefix");
        metadata(r,2,2);zero_slot(r,3);r.device=valid;stats.during_create=stats.during_spatial=stats.during_lock=stats.during_unlock={};require(r.catalog().source_return,"device failure left admission guard active");not_released(stats);
    }released(stats);}
    //All sample buffers/counts are read into locals before Lock, including each
    //new catalog item. Reentrant writes stay observable after normal completion.
    Stats stats;{CatalogRig r(stats,assets);r.selected(true);
        stats.during_lock=[&]{const auto slot=stats.create_calls;r.driver.samples[slot].buffer={0x7777888899990000ull+slot};r.storage.samples[slot].bytes=0;r.storage.samples[slot].ready=55;};
        require(r.catalog().source_return&&stats.unlock_calls==3,"later item reused the previous Lock metadata");
        for(unsigned i=1;i<=3;++i)require(r.driver.samples[i].buffer.value==0x7777888899990000ull+i&&r.storage.samples[i].bytes==0&&r.storage.samples[i].ready==55,"catalog rolled back callback writes");check_trace(stats);not_released(stats);
    }released(stats);
}
void exception_prefixes(const AssetStore& assets){
    unsigned cases=0;
    for(unsigned target:{1u,2u,3u})for(unsigned stage=0;stage<4;++stage)for(int point:{1,2}){Stats stats;{CatalogRig r(stats,assets);r.selected(true);
        stats.before_native=[&](unsigned at){if(stats.create_calls==target&&at==stage){if(at==0)stats.create_throw=point;if(at==1)stats.spatial_throw=point;if(at==2)stats.lock_throw=point;if(at==3)stats.unlock_throw=point;}};
        auto mutate=[&]{if(stats.create_calls==target){r.driver.samples[target].buffer={121};r.driver.samples[target].spatial={232};r.storage.samples[target].ready=454;}};
        if(stage==0)stats.during_create=mutate;if(stage==1)stats.during_spatial=mutate;if(stage==2)stats.during_lock=mutate;if(stage==3)stats.during_unlock=mutate;
        bool caught=false;try{(void)r.catalog();}catch(const Marker&){caught=true;}require(caught&&stats.create_calls==target,"exception was swallowed or suffix ran");
        const auto& current=r.driver.samples[target];require(current.buffer.value==(point==2?121u:(stage<2?0:stats.sample_identity))&&current.spatial.value==(point==2?232u:(stage<2?0:stats.spatial_identity))&&r.storage.samples[target].ready==(point==2?454u:1u),"exception rolled back callback writes/source prefix");
        for(unsigned slot=1;slot<target;++slot){metadata(r,slot,2);require(r.driver.samples[slot].buffer.value!=0,"prior completed item lost");}
        for(unsigned slot=target+1;slot<128;++slot)zero_slot(r,slot);
        require(stats.unlock_calls==target-1+(stage==3?1u:0u),"exception compensated Unlock");not_released(stats);
        stats.before_native={};stats.during_create=stats.during_spatial=stats.during_lock=stats.during_unlock={};stats.create_throw=stats.spatial_throw=stats.lock_throw=stats.unlock_throw=0;
        require(r.catalog().source_return&&stats.create_calls==target+3,"exception left catalog guard armed");not_released(stats);parents_unchanged(r);
    }released(stats);++cases;}require(cases==24,"exception prefix count differs");
}
void ownership_and_reset(const AssetStore& assets){
    Stats stats;{CatalogRig r(stats,assets);r.selected(true);require(r.single().source_return,"A9 preexisting registry setup failed");const auto previous=r.sample.buffer;
        require(r.catalog().source_return&&stats.sample_acquired==4&&stats.spatial_acquired==4,"catalog did not reuse existing sample owner");not_released(stats);
        require(r.single().source_return&&r.sample.buffer!=previous,"A9 failed after catalog");
        require(r.catalog().source_return&&stats.sample_acquired==8&&stats.spatial_acquired==8,"second catalog failed to reset source slots without releasing retained refs");not_released(stats);
        r.types.fill(77);const auto no_calls=r.catalog();require(no_calls.source_return&&no_calls.last_native_call==0&&!no_calls.last_native_result&&stats.create_calls==8,"empty selected catalog fabricated native activity");
        for(unsigned slot=0;slot<128;++slot)zero_slot(r,slot);not_released(stats);parents_unchanged(r);
    }released(stats);
    //QI can return the same valid interface with one acquired reference percall.
    Stats repeated;{CatalogRig r(repeated,assets);r.selected(true);repeated.during_spatial=[&]{repeated.reused_spatial=reinterpret_cast<Spatial*>(repeated.spatial_identity);};
        require(r.catalog().source_return&&repeated.spatial_live==1&&repeated.spatial_acquired==3,"same COM spatial token lost acquired references");
        require(r.driver.samples[1].spatial==r.driver.samples[2].spatial&&r.driver.samples[2].spatial==r.driver.samples[3].spatial,"shared spatial token truncated/replaced");not_released(repeated);
    }released(repeated);
}
struct Enumeration final:AudioEnumerationReceiver{bool visit(const std::optional<std::array<std::uint32_t,4>>&,std::string_view)override{throw std::runtime_error("reentry reached OS enumeration");}};
void admission_reentry(const AssetStore& assets){
    Stats stats;{CatalogRig r(stats,assets);r.selected(true);unsigned blocked=0;
        auto attempt=[&](auto&& f){try{f();}catch(const SourceAudioDefaultDeviceError&){++blocked;}};
        auto callback=[&]{AudioDeviceId d;AudioPrimaryId p;AudioBufferId b;Enumeration e;
            attempt([&]{(void)r.catalog();});attempt([&]{(void)r.single();});attempt([&]{(void)r.run();});
            attempt([&]{(void)r.owner->create_source_default_device(d);});attempt([&]{(void)r.owner->set_source_cooperative_level(r.device,1,3);});
            attempt([&]{(void)r.owner->create_source_primary_buffer(r.device,{20,0x11,0,0,{}},p);});
            attempt([&]{(void)r.owner->complete_source_primary_format(r.device,r.primary,r.rate,r.capability,r.enabled);});attempt([&]{(void)r.acquire();});
            attempt([&]{(void)r.owner->configure_source_listener_after_query(r.listener);});attempt([&]{(void)r.owner->release_device(r.device);});
            attempt([&]{r.owner->prepare_host_backend();});attempt([&]{(void)r.owner->create_buffer(r.device,{},b);});attempt([&]{(void)r.owner->release(r.sample.buffer);});attempt([&]{(void)r.owner->enumerate(e);});
        };stats.during_create=stats.during_spatial=stats.during_lock=stats.during_unlock=callback;
        require(r.catalog().source_return&&blocked==168&&stats.create_calls==3,"catalog admitted native callback reentry");
        std::atomic<bool> refused=false;std::thread other([&]{try{(void)r.catalog();}catch(const SourceAudioDefaultDeviceError&){refused=true;}});other.join();require(refused&&stats.create_calls==3,"foreign thread entered source catalog");
        not_released(stats);
    }released(stats);
    Stats cleanup;unsigned refused=0;{CatalogRig r(cleanup,assets);r.selected(true);require(r.catalog().source_return,"cleanup setup failed");auto* owner=r.owner.get();
        cleanup.during_cleanup=[&]{try{(void)owner->load_source_catalog(r.driver,r.storage,r.types,r.device,assets,r.enabled);}catch(const SourceAudioDefaultDeviceError&){++refused;}};
        r.owner.reset();cleanup.during_cleanup={};require(refused==6,"Release callback entered source catalog");
    }released(cleanup);
    Stats held;{Rig r(held);AudioDriverState driver;driver.initialized=51;driver.samples[1].type=27;AudioSampleLoadStorage storage;storage.samples[1].ready=63;std::array<std::uint32_t,72> types{};
        reject_at([&]{(void)load_source_audio_catalog_after_listener(SourceAudioSamplesBoundary{},*r.owner,driver,storage,types,r.device,assets,r.enabled);},0x461f50);
        require(held.create_calls==0&&driver.initialized==51&&driver.samples[1].type==27&&storage.samples[1].ready==63,"forged boundary reached source reset");
    }
    Stats failure;{CatalogRig r(failure,assets);SourceAudioListenerContinuationOutcome earlier=SourceAudioListenerAcquisitionFailure{0x461a97,S_FALSE};
        const auto result=load_source_audio_catalog_after_listener(earlier,*r.owner,r.driver,r.storage,r.types,r.device,assets,r.enabled);
        require(std::get<SourceAudioListenerAcquisitionFailure>(result).query_result==S_FALSE&&failure.create_calls==0&&r.storage.samples[1].ready==59,"prior failure mutated catalog");
    }released(failure);
}
void memory_prefixes(const AssetStore& assets){
    for(unsigned target:{1u,2u,3u})for(auto region:{Region::short_first,Region::split,Region::null_nonzero}){Stats stats;{CatalogRig r(stats,assets);r.selected(true);
        stats.before_native=[&](unsigned stage){if(stage==2)stats.region=stats.create_calls==target?region:Region::full;};
        bool caught=false;try{(void)r.catalog();}catch(const SourceAudioDefaultDeviceError& e){require(region==Region::null_nonzero&&e.source_site==0x461f0b,"wrong null memory boundary");caught=true;}catch(const std::out_of_range&){require(region!=Region::null_nonzero,"null nonzero span reached shared copy");caught=true;}
        require(caught&&stats.create_calls==target&&stats.lock_calls==target&&stats.unlock_calls==target-1,"invalid region did not stop at exact prefix");not_copied(stats);metadata(r,target,2);for(unsigned slot=target+1;slot<128;++slot)zero_slot(r,slot);not_released(stats);
    }released(stats);}
    Stats overflow;{CatalogRig r(overflow,assets);r.selected(true);overflow.during_create=[&]{if(overflow.create_calls==2)r.storage.samples[2].bytes=static_cast<std::uint32_t>(assets.resource(audio_sample_definitions[1].resource).size()-44+1);};
        bool caught=false;try{(void)r.catalog();}catch(const std::out_of_range&){caught=true;}
        require(caught&&overflow.create_calls==2&&overflow.lock_calls==2&&overflow.unlock_calls==1&&r.storage.samples[2].ready==1,"Main shared WAV bound moved or rolled back catalog prefix");not_copied(overflow);zero_slot(r,3);not_released(overflow);
    }released(overflow);
    for(auto output:{Output::zero,Output::unchanged}){Stats stats;stats.buffer_output=output;stats.create_hr=S_FALSE;{CatalogRig r(stats,assets);r.selected(true);require(r.catalog().source_return&&stats.create_calls==3&&!stats.lock_calls,"failed create empty output stopped ignored false path");not_released(stats);}released(stats);}
    Stats no_spatial;no_spatial.spatial_output=Output::zero;{CatalogRig r(no_spatial,assets);r.selected(true);require(r.catalog().source_return&&no_spatial.create_calls==3&&no_spatial.unlock_calls==3&&!no_spatial.spatial_acquired,"zero/null spatial output stopped source load");for(unsigned slot=1;slot<=3;++slot)require(r.driver.samples[slot].spatial.value==0,"null spatial fabricated native readiness");}released(no_spatial);
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected retail asset root");AssetStore assets(argv[1]);
    full_catalogs(assets);hresult_matrix(assets);mutations(assets);exception_prefixes(assets);ownership_and_reset(assets);admission_reentry(assets);memory_prefixes(assets);
    std::cout<<"source native catalog:7 groups,4 full retail modes,320 HRESULT matrices,24 exception prefixes,168 call reentries,6 cleanup reentries PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
