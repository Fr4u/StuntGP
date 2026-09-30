// CPU-only authored SDK interface fixture. No COM factory, DLL or system audio.
#define CINTERFACE
#include <windows.h>
#include <mmreg.h>
#include <dsound.h>
#include "sgn/source_audio_listener_configuration.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <iostream>
#include <type_traits>
#include <vector>

static_assert(sizeof(D3DVALUE)==4&&sizeof(HRESULT)==4&&DS3D_IMMEDIATE==0);
static_assert(offsetof(IDirectSound3DListenerVtbl,SetRolloffFactor)==15*sizeof(void*));
static_assert(offsetof(IDirectSound3DListenerVtbl,SetDopplerFactor)==12*sizeof(void*));
static_assert(offsetof(IDirectSound3DListenerVtbl,SetDistanceFactor)==11*sizeof(void*));
static_assert(sizeof(IDirectSound3DListenerVtbl)==18*sizeof(void*));
namespace {
using namespace sgn::retail;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Event {unsigned slot;std::uintptr_t receiver;std::uint32_t bits,apply;};
struct Native {
    IDirectSound3DListener object;
    std::vector<Event>* events;
    AudioListenerId* retained;
    Native* next;
    HRESULT result;
};
static_assert(std::is_standard_layout_v<Native>&&offsetof(Native,object)==0);
AudioListenerId identity(Native& native){return {reinterpret_cast<std::uintptr_t>(&native.object)};}
HRESULT call(IDirectSound3DListener* object,D3DVALUE value,DWORD apply,unsigned slot){
    auto& native=*reinterpret_cast<Native*>(object);
    native.events->push_back({slot,reinterpret_cast<std::uintptr_t>(object),std::bit_cast<std::uint32_t>(value),apply});
    if(native.next)*native.retained=identity(*native.next);
    return native.result;
}
HRESULT STDMETHODCALLTYPE rolloff(IDirectSound3DListener* self,D3DVALUE value,DWORD apply){return call(self,value,apply,15);}
HRESULT STDMETHODCALLTYPE doppler(IDirectSound3DListener* self,D3DVALUE value,DWORD apply){return call(self,value,apply,12);}
HRESULT STDMETHODCALLTYPE distance(IDirectSound3DListener* self,D3DVALUE value,DWORD apply){return call(self,value,apply,11);}
struct Adapter final:AudioListenerSetupDevice {
    std::array<Native*,3> admitted;
    explicit Adapter(std::array<Native*,3> objects):admitted(objects){}
    IDirectSound3DListener* resolve(AudioListenerId id){
        for(auto* native:admitted)if(identity(*native)==id)return &native->object;
        throw std::runtime_error("SDK fixture received a foreign or stale identity");
    }
    AudioResult query_listener(AudioPrimaryId,AudioListenerId&)override{throw std::runtime_error("SDK configuration queried interface");}
    AudioResult listener_rolloff(AudioListenerId id,float value,std::uint32_t apply)override{
        auto* native=resolve(id);return native->lpVtbl->SetRolloffFactor(native,value,apply);
    }
    AudioResult listener_doppler(AudioListenerId id,float value,std::uint32_t apply)override{
        auto* native=resolve(id);return native->lpVtbl->SetDopplerFactor(native,value,apply);
    }
    AudioResult listener_distance(AudioListenerId id,float value,std::uint32_t apply)override{
        auto* native=resolve(id);return native->lpVtbl->SetDistanceFactor(native,value,apply);
    }
};
void verify(HRESULT result){
    IDirectSound3DListenerVtbl table{};
    table.SetRolloffFactor=&rolloff;table.SetDopplerFactor=&doppler;table.SetDistanceFactor=&distance;
    std::vector<Event> events;AudioListenerId retained;
    Native a{{&table},&events,&retained,nullptr,result},b=a,c=a;
    a.next=&b;b.next=&c;retained=identity(a);Adapter adapter({&a,&b,&c});
    require(configure_source_audio_listener_461aa0(retained,adapter),"SDK HRESULT changed normal source return");
    const std::array<unsigned,3> slots{15,12,11};
    const std::array<std::uint32_t,3> bits{0x3f800000,0x40c00000,0x3c23d70a};
    const std::array<Native*,3> objects{&a,&b,&c};
    require(events.size()==3&&retained==identity(c),"SDK call count or retained owner differs");
    for(std::size_t i=0;i<events.size();++i)require(events[i].slot==slots[i]&&
        events[i].receiver==reinterpret_cast<std::uintptr_t>(&objects[i]->object)&&
        events[i].bits==bits[i]&&events[i].apply==DS3D_IMMEDIATE,"SDK calling convention, slots, arguments or receiver reload differ");
}
}
int main(){try{
    verify(S_OK);verify(S_FALSE);verify(E_FAIL);
    std::cout<<"LISTENER_CONFIGURATION_SDK_ABI_PASS:slots15/12/11;stdcall;3 HRESULT groups;receiver reloads;system audio calls0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
