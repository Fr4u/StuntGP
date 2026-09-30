#include "sgn/source_audio_listener_configuration.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <span>
#include <vector>

namespace {
using namespace sgn::retail;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Event {
    std::uint32_t slot;
    std::uint64_t listener;
    std::uint32_t value,apply;
    bool operator==(const Event&)const=default;
};
struct Marker {std::size_t step;};
struct Device final:AudioListenerSetupDevice {
    AudioListenerId& retained;
    std::array<AudioResult,3> results{};
    std::array<std::uint64_t,3> replacements{};
    std::vector<Event> events;
    std::size_t throw_after=3;
    bool replace=false;
    std::uint32_t retained_effects{};
    explicit Device(AudioListenerId& owner):retained(owner){}
    AudioResult query_listener(AudioPrimaryId,AudioListenerId&)override{
        throw std::runtime_error("configuration must not query/create/publish an interface");
    }
    AudioResult call(std::uint32_t slot,AudioListenerId id,float value,std::uint32_t apply){
        const auto index=events.size();require(index<3,"configuration repeated a setter");
        events.push_back({slot,id.value,std::bit_cast<std::uint32_t>(value),apply});
        retained_effects|=1u<<index; //authored callback writes survive HRESULT/exception
        if(replace)retained.value=replacements[index];
        if(index==throw_after)throw Marker{index};
        return results[index];
    }
    AudioResult listener_rolloff(AudioListenerId id,float value,std::uint32_t apply)override{return call(15,id,value,apply);}
    AudioResult listener_doppler(AudioListenerId id,float value,std::uint32_t apply)override{return call(12,id,value,apply);}
    AudioResult listener_distance(AudioListenerId id,float value,std::uint32_t apply)override{return call(11,id,value,apply);}
};
constexpr std::array<std::uint32_t,3> slots{15,12,11};
constexpr std::array<std::uint32_t,3> bits{0x3f800000,0x40c00000,0x3c23d70a};
constexpr std::array<std::uint32_t,3> null_sites{0x461aad,0x461abe,0x461ad0};
void check_events(const Device& device,std::span<const std::uint64_t> ids){
    require(device.events.size()==ids.size(),"wrong setter prefix length");
    for(std::size_t i=0;i<ids.size();++i)
        require(device.events[i]==Event{slots[i],ids[i],bits[i],0},"SDK slot/order, reloaded identity, float bits or immediate flag differ");
}
void all_results_ignored(){
    constexpr std::array<std::uint32_t,5> words{0,1,0xffffffff,0x80004005,0x7fffffff};
    for(const auto a:words)for(const auto b:words)for(const auto c:words){
        AudioListenerId listener{0x1122334455667788ull};Device device(listener);
        device.results={std::bit_cast<AudioResult>(a),std::bit_cast<AudioResult>(b),std::bit_cast<AudioResult>(c)};
        require(configure_source_audio_listener_461aa0(listener,device),"returned HRESULT became a false source result");
        const std::array<std::uint64_t,3> ids{listener.value,listener.value,listener.value};check_events(device,ids);
        require(device.retained_effects==7&&listener.value==0x1122334455667788ull,"callback effects or owner changed");
    }
}
void reloads_and_final_write(){
    AudioListenerId listener{0x100000001ull};Device device(listener);device.replace=true;
    device.replacements={0x200000002ull,0x300000003ull,0};
    require(configure_source_audio_listener_461aa0(listener,device),"final zero publication incorrectly checked after final call");
    const std::array<std::uint64_t,3> ids{0x100000001ull,0x200000002ull,0x300000003ull};check_events(device,ids);
    require(listener.value==0&&device.retained_effects==7,"final callback write not retained");
}
void null_at_each_dereference(){
    for(std::size_t stop=0;stop<3;++stop){
        AudioListenerId listener{stop==0?0ull:101ull};Device device(listener);device.replace=true;
        device.replacements={202,303,404};if(stop!=0)device.replacements[stop-1]=0;
        bool caught=false;
        try{(void)configure_source_audio_listener_461aa0(listener,device);}
        catch(const SourceAudioListenerConfigurationError& error){caught=true;require(error.source_site==null_sites[stop],"null rejected at wrong source instruction");}
        require(caught&&device.events.size()==stop&&listener.value==0&&device.retained_effects==((1u<<stop)-1u),
            "null rejection reset effects or called a later method");
        const std::array<std::uint64_t,3> ids{101,202,303};check_events(device,std::span(ids).first(stop));
    }
}
void exceptions_preserve_prefix(){
    for(std::size_t stop=0;stop<3;++stop){
        AudioListenerId listener{101};Device device(listener);device.replace=true;
        device.replacements={202,303,404};device.throw_after=stop;bool caught=false;
        try{(void)configure_source_audio_listener_461aa0(listener,device);}
        catch(const Marker& marker){caught=true;require(marker.step==stop,"exception identity was replaced");}
        require(caught&&listener.value==device.replacements[stop]&&device.retained_effects==((1u<<(stop+1))-1u),
            "exception swallowed or callback writes rolled back");
        const std::array<std::uint64_t,3> ids{101,202,303};check_events(device,std::span(ids).first(stop+1));
    }
}
}
int main(){try{
    all_results_ignored();reloads_and_final_write();null_at_each_dereference();exceptions_preserve_prefix();
    std::cout<<"LISTENER_CONFIGURATION_CORE_PASS:125 HRESULT triples;3 reloads;3 null sites;3 exception prefixes;no QueryInterface\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
catch(...){std::cerr<<"unexpected exception\n";return 1;}}
