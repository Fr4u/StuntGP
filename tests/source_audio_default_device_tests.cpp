#include "sgn/source_audio_default_device.hpp"
#include "sgn/source_audio_default_device_native.hpp"
#include "sgn/platform_audio.hpp"
#include <atomic>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>void rejects(std::uint32_t site,F&& f){
    try{f();}catch(const SourceAudioDefaultDeviceError& e){require(e.source_site==site,"wrong source site");return;}
    throw std::runtime_error("missing source admission error");
}
struct Stats {unsigned live{},adds{},releases{},other{};bool trap_release{};};
struct Native final:IDirectSound {
    Stats& stats;ULONG refs=1;
    explicit Native(Stats& s):stats(s){++stats.live;}
    ~Native(){--stats.live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** output)override{++stats.other;if(output)*output=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{++stats.adds;return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{
        if(stats.trap_release)std::_Exit(23);
        ++stats.releases;const auto remaining=--refs;if(!remaining)delete this;return remaining;
    }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC,LPDIRECTSOUNDBUFFER*,LPUNKNOWN)override{++stats.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS)override{++stats.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER,LPDIRECTSOUNDBUFFER*)override{++stats.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD)override{++stats.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Compact()override{++stats.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetSpeakerConfig(LPDWORD)override{++stats.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD)override{++stats.other;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(LPCGUID)override{++stats.other;return E_NOTIMPL;}
};
enum class Write {none,null,object};
struct Marker {int value;};
struct Script {
    Stats stats;Write write=Write::object;HRESULT result=S_OK;unsigned calls{};
    bool throws{};IDirectSound* last{};std::vector<IDirectSound**> slots;
};
thread_local Script* active=nullptr;
struct Scope {Script* previous;explicit Scope(Script& s):previous(active){active=&s;}~Scope(){active=previous;}};
HRESULT WINAPI scripted_create(LPCGUID guid,LPDIRECTSOUND* output,LPUNKNOWN outer){
    auto& s=*active;++s.calls;require(!guid&&!outer&&output,"actual source call arguments");
    require(*output==nullptr,"source output slot input was not preserved zero");s.slots.push_back(output);
    if(s.write==Write::null)*output=nullptr;
    else if(s.write==Write::object)*output=new Native(s.stats);
    s.last=*output;if(s.throws)throw Marker{73};return s.result;
}
const SourceAudioDefaultDeviceNativeApi scripted{&scripted_create};
void construction_and_input(){
    Script s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);
    require(!owner.host_backend_ready()&&!owner.source_device_identity()&&s.calls==0,"deferred construction invoked device API");
    rejects(0x462165,[&]{(void)owner.health();});rejects(0x462165,[&]{(void)owner.listener();});
    AudioDeviceId out{0x100000001ull};rejects(0x462146,[&]{(void)owner.create_source_default_device(out);});
    require(out.value==0x100000001ull&&s.calls==0,"invalid input token was truncated/reset or called API");
    rejects(0x462146,[&]{WindowsAudioDevice bad(DeferredAudioConstructionOnly{},0,SourceAudioDefaultDeviceNativeApi{nullptr});});
}
void success_retained_and_boundary(){
    Script s;Scope scope(s);AudioDeviceId out;
    {
        WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);
        const auto result=create_source_audio_default_device_462146({},out,owner);
        require(std::holds_alternative<SourceAudioCooperativeLevelBoundary>(result),"successful creation did not reach boundary");
        require(out.value==static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.last))&&out.value==owner.source_device_identity(),"native pointer-width output publication");
        require(s.stats.live==1&&s.stats.releases==0&&s.stats.adds==0&&!owner.host_backend_ready(),"creation probed/released or prepared a second backend");
        const auto token=out.value;
        rejects(0x462165,[&]{std::get<SourceAudioCooperativeLevelBoundary>(result).resume();});
        rejects(0x462165,[&]{(void)owner.cooperative_level(out,0,3);});
        rejects(0x462165,[&]{owner.prepare_host_backend();});
        rejects(0x462146,[&]{(void)create_source_audio_default_device_462146({},out,owner);});
        AudioDeviceId another;rejects(0x462146,[&]{(void)owner.create_source_default_device(another);});
        require(out.value==token&&another.value==0&&s.calls==1&&s.stats.releases==0&&s.stats.other==0,"boundary/second call changed retained ownership");
    }
    require(s.stats.live==0&&s.stats.releases==1&&out.value!=0,"host destruction did not drop exactly one ref or invented source zero store");
}
void failures_and_retry(){
    for(const HRESULT hr:{S_FALSE,E_FAIL})for(const Write write:{Write::none,Write::null,Write::object}){
        Script s;s.result=hr;s.write=write;Scope scope(s);AudioDeviceId out;
        {
            WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);
            const auto result=create_source_audio_default_device_462146({},out,owner);
            require(std::get<SourceAudioDefaultDeviceFailure>(result).result==static_cast<AudioResult>(hr),"nonzero HRESULT normalized");
            require(out.value==static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.last))&&out.value==owner.source_device_identity(),"error output discarded");
            require(s.stats.releases==0&&!owner.host_backend_ready(),"failure rolled back interface or created modern backend");
            if(write==Write::object){
                require(out.value!=0&&s.stats.live==1,"nonnull failure output not retained");
                rejects(0x462146,[&]{(void)owner.create_source_default_device(out);});
            }else{
                s.write=Write::object;s.result=S_OK;
                require(std::holds_alternative<SourceAudioCooperativeLevelBoundary>(create_source_audio_default_device_462146({},out,owner)),"failure with null output could not retry");
                require(s.calls==2&&s.slots[0]==s.slots[1],"retry replaced the native owner/output slot");
            }
        }
        require(s.stats.releases==1&&s.stats.live==0,"failure/retry owner reference leak");
    }
}
void exception_and_null_success(){
    Script s;s.throws=true;Scope scope(s);AudioDeviceId out;
    {
        WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);bool caught=false;
        try{(void)owner.create_source_default_device(out);}catch(const Marker& m){caught=m.value==73;}
        require(caught&&out.value==owner.source_device_identity()&&out.value!=0&&s.stats.releases==0,"entry exception lost output prefix");
    }
    require(s.stats.releases==1,"exception ownership leak");
    s.throws=false;s.write=Write::null;out={};
    WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);
    rejects(0x462162,[&]{(void)create_source_audio_default_device_462146({},out,owner);});
    require(out.value==0&&owner.source_device_identity()==0,"null success fabricated a native object");
}
void release_and_recreate(){
    Script s;Scope scope(s);AudioDeviceId out;
    {
        WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);
        (void)create_source_audio_default_device_462146({},out,owner);const auto token=out.value;
        rejects(0x4621ea,[&]{(void)owner.release_device({token+1});});
        require(owner.release_device(out)==0&&!owner.source_device_identity()&&out.value==token,"release result or independent source zero-store order");
        rejects(0x4621ea,[&]{(void)owner.release_device(out);});
        rejects(0x462146,[&]{(void)owner.create_source_default_device(out);});
        out={}; //Explicit caller store corresponding to4621ED, not native cleanup.
        (void)create_source_audio_default_device_462146({},out,owner);
        require(s.calls==2&&s.slots[0]==s.slots[1]&&s.stats.live==1,"recreation changed platform/native slot identity");
    }
    require(s.stats.releases==2&&s.stats.live==0,"recreate lifetime mismatch");
    Script external;Scope external_scope(external);
    WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);out={};
    (void)owner.create_source_default_device(out);auto* borrowed=external.last;
    require(borrowed->AddRef()==2&&owner.release_device(out)==1,"native Release refcount replaced with canned zero");
    require(owner.source_device_identity()==0&&external.stats.live==1,"owner retained a reference after release");
    require(borrowed->Release()==0,"external test reference cleanup");
}
void foreign_thread(){
    Script s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);AudioDeviceId out;
    (void)owner.create_source_default_device(out);const auto token=out.value;std::atomic<unsigned> rejected{};
    std::thread foreign([&]{
        const auto attempt=[&](auto&& f){try{f();}catch(const SourceAudioDefaultDeviceError&){++rejected;}};
        attempt([&]{(void)owner.create_source_default_device(out);});
        attempt([&]{(void)owner.release_device(out);});
        attempt([&]{owner.prepare_host_backend();});
        attempt([&]{(void)owner.host_backend_ready();});
        attempt([&]{(void)owner.source_device_identity();});
    });foreign.join();
    require(rejected==5&&s.calls==1&&out.value==token&&s.stats.releases==0,"foreign thread reached native owner or changed source state");
}
struct ExpectedDestruction {
    const WindowsAudioDevice* owner;
    const IDirectSound* native;
    const Stats* stats;
    std::uint64_t token;
    std::thread::id owner_thread;
};
struct ArmedDestruction {
    const ExpectedDestruction* expected{};
    const WindowsAudioDevice* target{};
    std::thread::id worker_thread{};
    bool armed{};
};
thread_local ArmedDestruction destruction;
[[noreturn]] void fail_termination()noexcept{std::_Exit(24);}
[[noreturn]] void expected_foreign_termination()noexcept{
    const auto& d=destruction;const auto* e=d.expected;
    const auto current=std::this_thread::get_id();
    if(!d.armed||!e||!e->owner||!e->native||!e->stats||d.target!=e->owner||
       current!=d.worker_thread||current==e->owner_thread||
       !e->token||e->token!=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(e->native))||
       !e->stats->trap_release||e->stats->live!=1||e->stats->releases!=0)
        std::_Exit(24);
    std::_Exit(0);
}
void foreign_destroy(std::string_view probe={}){
    //Fail by default, including constructor/import/setup or unrelated threads.
    //Install explicitly on the worker as well for CRTs with thread-local policy.
    std::set_terminate(&fail_termination);
    if(probe=="--death-probe-before-setup")std::terminate();
    Script s;Scope scope(s);
    auto* owner=new WindowsAudioDevice(DeferredAudioConstructionOnly{},0,scripted);AudioDeviceId out;
    const auto reached=create_source_audio_default_device_462146({},out,*owner);
    require(std::holds_alternative<SourceAudioCooperativeLevelBoundary>(reached)&&out.value!=0&&
        out.value==owner->source_device_identity()&&
        out.value==static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.last))&&
        !owner->host_backend_ready()&&s.calls==1&&s.stats.live==1&&s.stats.releases==0,
        "death receiving setup did not retain exactly the expected source interface");
    s.stats.trap_release=true; //Any native Release before termination exits23.
    const ExpectedDestruction expected{owner,s.last,&s.stats,out.value,std::this_thread::get_id()};
    if(probe=="--death-probe-main"){
        destruction={&expected,owner,std::this_thread::get_id(),true};
        std::set_terminate(&expected_foreign_termination);std::terminate();
    }
    std::thread wrong([&]{
        std::set_terminate(&fail_termination);
        destruction={&expected,owner,std::this_thread::get_id(),false};
        std::set_terminate(&expected_foreign_termination);
        if(probe=="--death-probe-unarmed-worker")std::terminate();
        if(probe=="--death-probe-other-worker"){
            std::thread unexpected([]{
                std::set_terminate(&expected_foreign_termination);std::terminate();
            });unexpected.join();std::_Exit(1);
        }
        if(probe=="--death-probe-wrong-identity"){
            destruction.worker_thread=expected.owner_thread;destruction.armed=true;std::terminate();
        }
        if(probe=="--death-probe-wrong-object"){
            destruction.target=nullptr;destruction.armed=true;std::terminate();
        }
        //No work between arming and deleting this specific retained object.
        destruction.armed=true;
        delete owner;
        destruction.armed=false;std::_Exit(1);
    });wrong.join();std::_Exit(1);
}
struct Actual {unsigned calls{};IDirectSound* before{};IDirectSound* after{};HRESULT result=E_UNEXPECTED;};
thread_local Actual* actual=nullptr;
HRESULT WINAPI recording_actual_create(LPCGUID guid,LPDIRECTSOUND* output,LPUNKNOWN outer){
    require(!guid&&!outer&&output,"actual OS source arguments");auto& a=*actual;++a.calls;a.before=*output;
    const auto hr=::DirectSoundCreate(guid,output,outer);a.after=*output;a.result=hr;return hr;
}
int actual_os(){
    Actual observed;actual=&observed;AudioDeviceId out;
    WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,SourceAudioDefaultDeviceNativeApi{&recording_actual_create});
    const auto result=create_source_audio_default_device_462146({},out,owner);
    require(observed.calls==1&&observed.before==nullptr,"actual import input/call count");
    require(out.value==static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(observed.after))&&out.value==owner.source_device_identity(),"actual OS output not retained");
    require(!owner.host_backend_ready(),"source OS receiving created modern backend");
    std::cout<<"actual DirectSoundCreate HRESULT="<<observed.result<<" pointer_token="<<out.value<<'\n';
    if(observed.result!=0){
        require(std::get<SourceAudioDefaultDeviceFailure>(result).result==static_cast<AudioResult>(observed.result),"actual HRESULT altered");
        std::cerr<<"OS DEVICE CREATION FAILED: output/error retained, success/lifetime receiving NOT qualified\n";return 2;
    }
    require(std::holds_alternative<SourceAudioCooperativeLevelBoundary>(result)&&out.value!=0,"actual success missing typed boundary");
    rejects(0x462165,[&]{std::get<SourceAudioCooperativeLevelBoundary>(result).resume();});
    const auto token=out.value;const auto remaining=owner.release_device(out);
    require(owner.source_device_identity()==0&&out.value==token,"actual release did not preserve source store ordering");
    std::cout<<"actual owner Release result="<<remaining<<"; caller source token retained\n";out={};
    actual=nullptr;return 0;
}
void preview_compatibility(){
    Script s;Scope scope(s);WindowsAudioDevice owner(DeferredAudioConstructionOnly{},0,scripted);
    require(!owner.host_backend_ready()&&s.calls==0,"preview wrapper was prepared by construction");
    owner.prepare_host_backend();
    require(owner.host_backend_ready()&&owner.health()==0&&owner.info().channels>0&&owner.info().sample_rate>0&&owner.live_buffers()==0,"actual deferred XAudio2 preparation failed");
    owner.prepare_host_backend();
    AudioDeviceId out;require(owner.create_default_device(out)==0&&out.value==1&&owner.listener().value==1,"existing preview generation/listener contract changed");
    require(owner.release_device(out)==0,"existing preview release failed");
    AudioDeviceId source;rejects(0x462146,[&]{(void)owner.create_source_default_device(source);});
    require(source.value==0&&s.calls==0&&owner.source_device_identity()==0,"prepared preview called DirectSound source creation");
    std::cout<<"actual deferred preview compatibility PASS\n";
}
}
int main(int argc,char** argv){try{
    require(argc==2,"expected --abi, --os, --preview, or --foreign-destroy");const std::string mode=argv[1];
    if(mode=="--abi"){
        construction_and_input();success_retained_and_boundary();failures_and_retry();
        exception_and_null_success();release_and_recreate();foreign_thread();
        std::cout<<"source default device: 6 ABI/lifetime groups PASS\n";return 0;
    }
    if(mode=="--os")return actual_os();
    if(mode=="--preview"){preview_compatibility();return 0;}
    if(mode=="--foreign-destroy")foreign_destroy();
    if(mode=="--death-probe-before-setup"||mode=="--death-probe-main"||mode=="--death-probe-unarmed-worker"||
       mode=="--death-probe-other-worker"||mode=="--death-probe-wrong-identity"||mode=="--death-probe-wrong-object")foreign_destroy(mode);
    throw std::runtime_error("unknown mode");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
