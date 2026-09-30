#include "sgn/source_audio_primary_format.hpp"
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
    HRESULT caps_hr=S_OK,get_hr=S_OK,set_hr=S_OK;
    DWORD flags=8,max_3d=8,written=18;
    WAVEFORMATEX returned{0x1234,3,11025,33075,3,8,19},submitted{};
    LPWAVEFORMATEX format_address{};LPDSCAPS caps_address{};
    unsigned caps_calls{},get_calls{},set_calls{};
    std::function<void()> during_caps,during_get,during_set;
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
    HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX out,DWORD count,LPDWORD written)override{
        ++s.get_calls;s.events.push_back("get_format");
        require(out&&count==18&&written,"source GetFormat arguments differ");
        s.format_address=out;*out=s.returned;*written=s.written;
        if(s.during_get)s.during_get();return s.get_hr;
    }
    HRESULT STDMETHODCALLTYPE GetVolume(LPLONG)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetPan(LPLONG)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND,LPCDSBUFFERDESC)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Lock(DWORD,DWORD,LPVOID*,LPDWORD,LPVOID*,LPDWORD,DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE Play(DWORD,DWORD,DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD)override{return unexpected();}
    HRESULT STDMETHODCALLTYPE SetFormat(LPCWAVEFORMATEX value)override{
        ++s.set_calls;s.events.push_back("set_format");
        require(value==s.format_address,"SetFormat did not receive the same native GetFormat scratch object");
        s.submitted=*value;if(s.during_set)s.during_set();return s.set_hr;
    }
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
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS out)override{
        ++s.caps_calls;s.events.push_back("caps");require(out&&out->dwSize==96,"source device caps argument differs");
        s.caps_address=out;out->dwFlags=s.flags;out->dwMaxHw3DAllBuffers=s.max_3d;
        if(s.during_caps)s.during_caps();return s.caps_hr;
    }
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
SourceAudioDeviceCapsBoundary reach_caps(WindowsAudioDevice& owner,AudioDeviceId& id,AudioPrimaryId& output){
    const auto a3=reach(owner,id,output);
    const auto a4=create_source_audio_primary_461d25(a3,owner);
    return std::get<SourceAudioDeviceCapsBoundary>(a4);
}
template<class F>void reject_at(F&& f,std::uint32_t site){
    bool caught=false;try{f();}catch(const SourceAudioDefaultDeviceError& e){caught=true;require(e.source_site==site,"wrong typed source boundary");}
    require(caught,"missing typed source boundary");
}
void native_result_order_and_prefix_matrix(){
    for(const DWORD maximum:{7u,8u})for(const HRESULT caps:{S_OK,S_FALSE,E_FAIL})
    for(const HRESULT get:{S_OK,S_FALSE,E_FAIL})for(const HRESULT set:{S_OK,S_FALSE,E_FAIL}){
        Stats s;s.max_3d=maximum;s.caps_hr=caps;s.get_hr=get;s.set_hr=set;s.cooperative_hr=E_FAIL;Scope scope(s);
        AudioDeviceId id;AudioPrimaryId primary;std::uint32_t rate=44100,capability=73,enabled=29;
        {
            WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);const auto a4=reach_caps(owner,id,primary);
            const auto result=complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);
            const auto expected_site=caps!=0?0x461d4cu:(get!=0?0x461d7eu:0x461deau);
            const auto expected_hr=caps!=0?caps:(get!=0?get:set);
            require(result.last_call==expected_site&&result.result==expected_hr&&result.source_return()==(caps==0&&get==0&&set==0),
                "tested HRESULT branch or source Boolean return differs");
            require(s.caps_calls==1&&s.get_calls==unsigned(caps==0)&&s.set_calls==unsigned(caps==0&&get==0),"native call order/count differs");
            require(capability==(caps==0&&maximum<8?0u:73u)&&enabled==(caps==0&&maximum<8?0u:29u),"reached spatial clears lost or applied before caps success");
            require(primary.value==s.primary_identity&&id.value==s.device_identity&&owner.source_primary_identity()==primary.value&&
                owner.source_device_identity()==id.value&&s.adds==0&&s.other==0&&s.primary_releases==0&&s.device_releases==0&&!owner.host_backend_ready(),
                "format setup replaced/released owners or prepared unrelated backend");
            reject_at([&]{(void)complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);},0x461d4cu);
        }
        require(s.primary_releases==1&&s.device_releases==1&&s.bad_release_order==0,"format result path changed child/parent cleanup");
        std::vector<std::string_view> expected{"create","cooperative","primary","caps"};
        if(caps==0)expected.push_back("get_format");
        if(caps==0&&get==0)expected.push_back("set_format");
        expected.push_back("primary_release");expected.push_back("device_release");require(s.events==expected,"full native event order differs");
    }
}
void exact_format_arithmetic_and_ignored_written(){
    struct Case {DWORD flags;WORD channels;std::uint32_t rate;WORD bits,align;DWORD bytes;};
    const std::array<Case,8> cases{{
        {0,1,44100,8,1,44100},{8,1,44100,16,2,88200},{0xfffffff7u,3,11025,8,3,33075},
        {0xffffffffu,3,11025,16,6,66150},{8,65535,44100,16,65534,2890049400u},
        {0,0,0xffffffffu,8,0,0},{8,2,0xffffffffu,16,4,0xfffffffcu},{8,32768,0x80000000u,16,0,0}
    }};
    for(const auto& item:cases)for(const DWORD written:{0u,1u,16u,18u,40u,0xffffffffu}){
        Stats s;s.flags=item.flags;s.returned.nChannels=item.channels;s.written=written;s.max_3d=0xffffffffu;Scope scope(s);
        WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
        const auto a4=reach_caps(owner,id,primary);std::uint32_t rate=item.rate,capability=1,enabled=2;
        const auto result=complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);const auto& f=s.submitted;
        require(result.source_return()&&s.set_calls==1&&f.wFormatTag==1&&f.nChannels==item.channels&&f.nSamplesPerSec==item.rate&&
            f.wBitsPerSample==item.bits&&f.nBlockAlign==item.align&&f.nAvgBytesPerSec==item.bytes&&f.cbSize==19,
            "PCM/rate/bitmask/WORD truncation/DWORD wrap or untouched channels/cbSize differs");
        require(capability==1&&enabled==2,"unsigned large max3D count was treated as negative");
    }
}
void source_reads_after_native_get_format(){
    Stats s;s.flags=0;s.max_3d=7;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);
    AudioDeviceId id;AudioPrimaryId primary;const auto a4=reach_caps(owner,id,primary);
    std::uint32_t rate=44100,capability=9,enabled=10;
    s.during_caps=[&]{require(capability==9&&enabled==10,"spatial state changed before native caps returned");};
    s.during_get=[&]{require(capability==0&&enabled==0,"source spatial clears did not precede GetFormat");rate=5512;s.caps_address->dwFlags=8;};
    const auto result=complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);
    require(result.source_return()&&s.submitted.nSamplesPerSec==5512&&s.submitted.wBitsPerSample==16&&
        s.submitted.nAvgBytesPerSec==33072,"rate/caps flags were eagerly snapshotted before GetFormat");
}
void actual_primary_history_and_null_dereference(){
    for(const HRESULT creation:{S_FALSE,E_FAIL}){
        Stats s;s.primary_hr=creation;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);
        AudioDeviceId id;AudioPrimaryId primary;const auto a3=reach(owner,id,primary);(void)create_source_audio_primary_461d25(a3,owner);
        const SourceAudioDeviceCapsBoundary forged{id,primary};std::uint32_t rate=44100,capability=3,enabled=4;
        reject_at([&]{(void)complete_source_audio_primary_format_461d4c(forged,owner,rate,capability,enabled);},0x461d4cu);
        require(s.caps_calls==0&&capability==3&&enabled==4,"nonnull failed primary granted native caps continuation");
    }
    for(const HRESULT caps:{S_OK,E_FAIL}){
        Stats s;s.publish=false;s.caps_hr=caps;s.max_3d=7;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);
        AudioDeviceId id;AudioPrimaryId primary;const auto a4=reach_caps(owner,id,primary);std::uint32_t rate=44100,capability=3,enabled=4;
        if(caps==0){
            reject_at([&]{(void)complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);},0x461d78u);
            require(capability==0&&enabled==0,"null-primary boundary rolled back source clears");
        }else{
            const auto result=complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);
            require(result.result==E_FAIL&&result.last_call==0x461d4c&&!result.source_return()&&capability==3&&enabled==4,"caps failure was replaced by earlier null-primary check");
        }
        require(s.caps_calls==1&&s.get_calls==0&&s.set_calls==0,"null source primary was dereferenced or blocked before device caps");
    }
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
    const auto a3=reach(owner,id,primary);std::uint32_t rate=44100,capability=1,enabled=1;
    const SourceAudioDeviceCapsBoundary forged{id,primary};
    reject_at([&]{(void)complete_source_audio_primary_format_461d4c(forged,owner,rate,capability,enabled);},0x461d4cu);
    const auto result=create_source_audio_primary_461d25(a3,owner);const auto& a4=std::get<SourceAudioDeviceCapsBoundary>(result);
    const SourceAudioDeviceCapsBoundary wrong_size{id,primary,92};
    reject_at([&]{(void)complete_source_audio_primary_format_461d4c(wrong_size,owner,rate,capability,enabled);},0x461d4cu);
    require(complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled).source_return(),"invalid prefix consumed actual valid continuation");
}
void primary_reload_boundaries(){
    for(const bool after_format:{false,true}){
        Stats s;s.max_3d=7;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
        const auto a4=reach_caps(owner,id,primary);std::uint32_t rate=44100,capability=4,enabled=5;
        auto& hook=after_format?s.during_get:s.during_caps;hook=[&]{++primary.value;};
        reject_at([&]{(void)complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);},after_format?0x461de8u:0x461d78u);
        require(capability==0&&enabled==0&&s.get_calls==unsigned(after_format)&&s.set_calls==0&&
            s.primary_releases==0&&s.device_releases==0,"source primary reload/prefix lifetime differs");
    }
}
void native_exceptions_reentry_and_foreign_thread(){
    for(const unsigned at:{0u,1u,2u}){
        Stats s;s.max_3d=7;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
        const auto a4=reach_caps(owner,id,primary);std::uint32_t rate=44100,capability=9,enabled=8;
        auto& hook=at==0?s.during_caps:(at==1?s.during_get:s.during_set);hook=[] {throw Marker{83};};bool caught=false;
        try{(void)complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);}catch(const Marker& m){caught=m.value==83;}
        require(caught&&capability==(at==0?9u:0u)&&enabled==(at==0?8u:0u)&&s.primary_releases==0&&s.device_releases==0,
            "native exception rolled back reached writes or native ownership");
        reject_at([&]{(void)complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);},0x461d4cu);
        (void)owner.set_source_cooperative_level(id,1,3); //The activity guard must have cleared on unwind.
    }
    Stats s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,api);AudioDeviceId id;AudioPrimaryId primary;
    const auto a4=reach_caps(owner,id,primary);std::uint32_t rate=44100,capability=1,enabled=1;std::atomic<bool> refused{};
    std::thread worker([&]{try{(void)complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);}catch(const SourceAudioDefaultDeviceError&){refused=true;}});
    worker.join();require(refused&&s.caps_calls==0,"foreign thread reached native setup");
    unsigned blocked=0;const auto reenter=[&]{
        AudioDeviceId other;AudioPrimaryId other_primary;
        try{(void)owner.create_source_default_device(other);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)owner.create_source_primary_buffer(id,exact_request(),other_primary);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)owner.set_source_cooperative_level(id,1,3);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)owner.release_device(id);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
        try{(void)complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled);}catch(const SourceAudioDefaultDeviceError&){++blocked;}
    };
    s.during_caps=reenter;s.during_get=reenter;s.during_set=reenter;
    require(complete_source_audio_primary_format_461d4c(a4,owner,rate,capability,enabled).source_return()&&blocked==15,
        "native setup admitted reentrant owner/native operations");
}
}
int main(){try{
    native_result_order_and_prefix_matrix();exact_format_arithmetic_and_ignored_written();source_reads_after_native_get_format();
    actual_primary_history_and_null_dereference();primary_reload_boundaries();native_exceptions_reentry_and_foreign_thread();
    std::cout<<"source primary format: 6 groups, 54 HRESULT/prefix cases, 48 format/count cases PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
