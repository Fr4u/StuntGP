#include "sgn/audio_environment.hpp"
#include <fstream>
#include <iostream>

using namespace sgn;
using namespace sgn::retail;
namespace {
static_assert(sizeof(AudioEnvironmentState)==2372);
template<class T> void read(std::istream& in,T& value){in.read(reinterpret_cast<char*>(&value),sizeof(value));if(!in)throw std::runtime_error("short environment input");}
template<class T> void write(std::ostream& out,const T& value){out.write(reinterpret_cast<const char*>(&value),sizeof(value));}
struct Device final:AudioEnvironmentDevice {
    std::array<std::uint32_t,5> response{};
    std::vector<std::uint32_t> events;
    void event(std::uint32_t op,std::uint64_t id,const AudioPropertyGuid& guid){
        events.push_back(op);events.push_back(static_cast<std::uint32_t>(id));
        const auto words=std::bit_cast<std::array<std::uint32_t,4>>(guid);events.insert(events.end(),words.begin(),words.end());
    }
    AudioResult query_audio_property(AudioSpatialId id,const AudioPropertyGuid& guid,AudioPropertyId& out) override{
        event(1,id.value,guid);events.push_back(static_cast<std::uint32_t>(out.value));out.value=response[1];return std::bit_cast<std::int32_t>(response[0]);
    }
    AudioResult audio_property_support(AudioPropertyId id,const AudioPropertyGuid& guid,std::uint32_t prop,std::uint32_t& out) override{
        event(2,id.value,guid);events.push_back(prop);events.push_back(out);out=response[3];return std::bit_cast<std::int32_t>(response[2]);
    }
    AudioResult set_audio_property(AudioPropertyId id,const AudioPropertyGuid& guid,std::uint32_t prop,std::span<const std::uint8_t> instance,std::span<const std::uint8_t> data) override{
        event(3,id.value,guid);events.push_back(prop);events.push_back(static_cast<std::uint32_t>(instance.size()));events.push_back(static_cast<std::uint32_t>(data.size()));
        if(!instance.empty()||data.size()!=56)throw std::runtime_error("bad property payload");
        for(std::size_t i=0;i<data.size();i+=4){std::array<std::uint8_t,4> b{data[i],data[i+1],data[i+2],data[i+3]};events.push_back(std::bit_cast<std::uint32_t>(b));}
        return std::bit_cast<std::int32_t>(response[4]);
    }
};
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("expected input and output corpus");
    std::ifstream in(argv[1],std::ios::binary);std::ofstream out(argv[2],std::ios::binary|std::ios::trunc);
    if(!in||!out)throw std::runtime_error("environment corpus open failed");
    std::array<char,8> magic{};read(in,magic);if(std::string_view(magic.data(),8)!="SGNAENV1")throw std::runtime_error("bad environment schema");
    write(out,magic);std::uint32_t count{};read(in,count);write(out,count);
    for(std::uint32_t i=0;i<count;++i){
        std::uint32_t op{},argument{},enabled{};AudioEnvironmentState state;AudioDriverState driver;Device device;Vec3 point;
        read(in,op);read(in,state);read(in,driver.initialized);read(in,enabled);
        std::uint32_t spatial{};read(in,spatial);driver.samples[1].spatial.value=spatial;
        read(in,argument);read(in,device.response);read(in,point);
        std::uint32_t length{};read(in,length);if(length>1024*1024)throw std::runtime_error("config too long");
        Bytes cfg(length);in.read(reinterpret_cast<char*>(cfg.data()),length);if(!in)throw std::runtime_error("short config");
        AudioEnvironmentWorld world{state,driver,enabled,device};std::uint32_t result=0;
        switch(op){
        case 0:result=set_audio_environment(world,argument);break;
        case 1:result=audio_environment_support(world,{device.response[1]},argument);break;
        case 2:initialize_audio_environments(world);break;
        case 3:update_audio_environment(world,point);break;
        case 4:load_audio_environments(cfg,world);break;
        case 5:result=audio_environment_contains(state.regions[1],point);break;
        case 6:state=initial_audio_environment_state();break;
        default:throw std::runtime_error("unknown environment operation");
        }
        write(out,result);write(out,state);write(out,static_cast<std::uint32_t>(device.events.size()));
        for(auto word:device.events)write(out,word);
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing environment input");
    if(!out)throw std::runtime_error("environment output write failed");
    std::cout<<"environment cases "<<count<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
