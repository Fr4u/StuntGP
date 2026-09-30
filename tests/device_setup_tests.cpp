#include "sgn/retail_device_setup.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sgn::retail;
namespace {
std::uint64_t checks{};
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class A,class B> void check_equal(const A& actual,const B& expected,const char* text){++checks;require(actual==expected,text);}
struct Reader {
    std::ifstream file;
    explicit Reader(const char* path):file(path,std::ios::binary){require(bool(file),"cannot open device corpus");}
    std::uint32_t word(){
        std::array<unsigned char,4> bytes{};file.read(reinterpret_cast<char*>(bytes.data()),4);
        require(file.gcount()==4,"truncated device corpus");
        return std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
    }
    std::uint32_t count(std::uint32_t limit){const auto n=word();require(n<=limit,"device corpus count exceeds bound");return n;}
    std::vector<std::uint32_t> words(std::uint32_t n){std::vector<std::uint32_t> result;for(std::uint32_t i=0;i<n;++i)result.push_back(word());return result;}
};
struct Event {
    std::uint32_t receiver,slot;
    std::vector<std::uint32_t> payload;
    std::uint32_t hresult;
    std::array<std::uint32_t,6> before;
};
struct ExternalState {
    std::array<std::uint32_t,6> globals{};
    std::array<std::uint32_t,2> scenes{};
    std::map<std::vector<std::uint32_t>,std::uint32_t> states;
    std::optional<std::array<std::uint32_t,3>> surface_key;
    std::vector<Event> events;
    std::uint32_t fail_slot{0xffffffffu},fail_result{};
    std::uint32_t call(std::uint32_t receiver,std::uint32_t slot,std::vector<std::uint32_t> payload){
        const auto result=slot==fail_slot?fail_result:0u;
        events.push_back({receiver,slot,payload,result,globals});
        if(result!=0)return result;
        if(slot==0x24){require(receiver<2&&scenes[receiver]==0,"nested native fixture BeginScene");scenes[receiver]=1;}
        else if(slot==0x28){require(receiver<2&&scenes[receiver]==1,"native fixture End without BeginScene");scenes[receiver]=0;}
        else if(slot==0x74){require(receiver==2&&payload.size()==3,"surface receiver/payload");surface_key=std::array{payload[0],payload[1],payload[2]};}
        else{
            require(receiver<2&&!payload.empty(),"device setter receiver/payload");
            const auto value=payload.back();payload.pop_back();std::vector<std::uint32_t> key{receiver,slot};
            key.insert(key.end(),payload.begin(),payload.end());states[key]=value;
        }
        return result;
    }
};
struct Device final:DeviceSetupDevice {
    ExternalState& state;std::uint32_t identity;
    Device(ExternalState& s,std::uint32_t i):state(s),identity(i){}
    std::uint32_t begin_scene() override{return state.call(identity,0x24,{});}
    std::uint32_t end_scene() override{return state.call(identity,0x28,{});}
    std::uint32_t set_render_state(std::uint32_t a,std::uint32_t b) override{return state.call(identity,0x58,{a,b});}
    std::uint32_t set_light_state(std::uint32_t a,std::uint32_t b) override{return state.call(identity,0x60,{a,b});}
    std::uint32_t set_texture_stage_state(std::uint32_t a,std::uint32_t b,std::uint32_t c) override{return state.call(identity,0xa0,{a,b,c});}
};
struct Surface final:DeviceSetupSurface {
    ExternalState& state;
    explicit Surface(ExternalState& s):state(s){}
    std::uint32_t set_color_key(std::uint32_t flags,std::span<const std::uint32_t,2> key) override{return state.call(2,0x74,{flags,key[0],key[1]});}
};
DeviceSetupGlobals globals(ExternalState& state){auto& g=state.globals;return {g[0],g[1],g[2],g[3],g[4],g[5]};}
std::uint32_t invoke(std::uint32_t entry,DeviceSetupWorld world){
    switch(entry){
    case 0x42fa80:return initialize_retail_device(world);
    case 0x42f9c0:configure_device_filter_42f9c0(world);break;
    case 0x42f930:configure_device_color_key_42f930(world);break;
    case 0x42fa10:configure_device_texture_42fa10(world);break;
    case 0x42fa00:device_setup_noop_42fa00();break;
    case 0x42fa40:configure_device_multitexture_42fa40(world);break;
    default:throw std::runtime_error("unknown source device function");
    }
    return 0xffffffffu; //void helpers: sourceEAX is not a defined result contract
}
std::uint32_t host_guards(){
    ExternalState state;state.globals={1,2,3,4,5,6};const auto original=state.globals;
    Device device(state,0);DeviceSetupDevice* initial=nullptr;DeviceSetupDevice* global=nullptr;DeviceSetupSurface* surface=nullptr;
    std::uint32_t e4{},e8{},f0{2};std::uint16_t units{},stages{};
    DeviceSetupWorld world{{e4,e8,f0,units,stages,initial},globals(state),global,surface};
    std::uint32_t rejected=0;
    try{initialize_retail_device(world);}catch(const std::invalid_argument&){++rejected;}
    check_equal(state.events.size(),std::size_t{0},"missing initial receiver performed calls");check_equal(state.globals,original,"missing initial receiver wrote flags");
    initial=&device;state.fail_slot=0x24;state.fail_result=1;
    check_equal(initialize_retail_device(world),0u,"failed Begin must precede use of absent global receiver");
    check_equal(state.events.size(),std::size_t{1},"failed Begin emitted extra call");check_equal(state.globals,original,"failed Begin changed flags");
    state.events.clear();state.fail_slot=0xffffffffu;
    try{configure_device_filter_42f9c0(world);}catch(const std::invalid_argument&){++rejected;}
    check_equal(state.events.size(),std::size_t{0},"missing global receiver performed calls");check_equal(state.globals,original,"missing global receiver wrote flags");
    //Absent surface is a valid source branch even with no global device.
    configure_device_color_key_42f930(world);check_equal(state.globals,original,"absent surface changed flags");
    check_equal(rejected,2u,"host receiver rejection count");return rejected;
}
}
int main(int argc,char** argv){
    try{
        require(argc==2,"device_setup_tests requires corpus path");Reader in(argv[1]);check_equal(in.word(),0x31555344u,"device corpus magic");
        const auto groups=in.count(1000);require(groups!=0,"empty device corpus");
        std::uint64_t calls=0,parent_calls=0,helper_calls=0,event_count=0,state_count=0,global_words=0,event_global_words=0,failed_parent=0;
        for(std::uint32_t group=0;group<groups;++group){
            ExternalState state;for(auto& word:state.globals)word=in.word();
            Device primary(state,0),other(state,1);Surface surface(state);
            DeviceSetupDevice* initial=&primary;DeviceSetupDevice* global=&primary;DeviceSetupSurface* current_surface=nullptr;
            std::uint32_t e4{},e8{},f0{};std::uint16_t units{},stages{};
            DeviceSetupWorld world{{e4,e8,f0,units,stages,initial},globals(state),global,current_surface};
            const auto count=in.count(10);require(count!=0,"empty retained scenario");
            for(std::uint32_t turn=0;turn<count;++turn){
                const auto entry=in.word();e4=in.word();e8=in.word();f0=in.word();
                const auto raw_units=in.word(),raw_stages=in.word();require(raw_units<=65535&&raw_stages<=65535,"capability WORD overflow");
                units=static_cast<std::uint16_t>(raw_units);stages=static_cast<std::uint16_t>(raw_stages);
                current_surface=in.count(1)?&surface:nullptr;global=in.count(1)?&primary:&other;
                state.fail_slot=in.word();state.fail_result=in.word();const auto expected_result=in.word();
                std::array<std::uint32_t,6> expected_globals;for(auto& word:expected_globals)word=in.word();
                state.events.clear();check_equal(invoke(entry,world),expected_result,"source/native return mismatch");
                for(std::size_t i=0;i<6;++i){check_equal(state.globals[i],expected_globals[i],"retained source global mismatch");++global_words;}
                ++calls;if(entry==0x42fa80){++parent_calls;if(expected_result==0)++failed_parent;}else ++helper_calls;
                const auto events=in.count(200);check_equal(state.events.size(),std::size_t{events},"external event count mismatch");event_count+=events;
                for(const auto& event:state.events){
                    check_equal(event.receiver,in.word(),"external receiver identity mismatch");check_equal(event.slot,in.word(),"external slot/order mismatch");
                    const auto payload=in.count(3);check_equal(event.payload,in.words(payload),"external arguments mismatch");
                    check_equal(event.hresult,in.word(),"external HRESULT mismatch");
                    for(auto before:event.before){check_equal(before,in.word(),"source globals at external-call boundary mismatch");++event_global_words;}
                }
                const auto states=in.count(300);check_equal(state.states.size(),std::size_t{states},"retained explicit API state count mismatch");state_count+=states;
                for(const auto& [actual_key,actual_value]:state.states){
                    std::vector<std::uint32_t> expected_key{in.word(),in.word()};const auto selectors=in.count(2);const auto values=in.words(selectors);
                    expected_key.insert(expected_key.end(),values.begin(),values.end());check_equal(actual_key,expected_key,"retained API receiver/key mismatch");
                    check_equal(actual_value,in.word(),"retained API value mismatch");
                }
                for(auto active:state.scenes)check_equal(active,in.word(),"retained scene-active state mismatch");
                const auto has_key=in.count(1);check_equal(state.surface_key.has_value(),has_key!=0,"retained surface key presence mismatch");
                if(has_key)for(auto word:*state.surface_key)check_equal(word,in.word(),"retained surface key value mismatch");
            }
        }
        require(in.file.peek()==std::char_traits<char>::eof(),"trailing device corpus data");const auto rejected=host_guards();
        std::cout<<"{\"groups\":"<<groups<<",\"calls\":"<<calls<<",\"parent_calls\":"<<parent_calls<<",\"helper_calls\":"<<helper_calls
            <<",\"source_global_words\":"<<global_words<<",\"event_global_words\":"<<event_global_words<<",\"events\":"<<event_count
            <<",\"explicit_state_entries\":"<<state_count<<",\"failed_parent_calls\":"<<failed_parent<<",\"host_rejections\":"<<rejected<<",\"checks\":"<<checks<<"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
