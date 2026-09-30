#include "sgn/retail_audio_catalog_startup.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace sgn;
using namespace sgn::retail;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Boundary final:AudioCreationDevice {
    enum class Mode {lock_retention,catalog_mode,catalog_device};
    Mode mode;
    AudioSample* sample{};AudioSampleLoadFields* fields{};
    std::uint32_t* enabled{};AudioDeviceId* device{};
    std::vector<AudioDeviceId> creates;
    Bytes memory;
    AudioBufferId unlocked{};
    std::size_t unlock_bytes{};
    std::uint32_t locked_bytes{};
    explicit Boundary(Mode value):mode(value){}
    AudioResult create_buffer(AudioDeviceId id,const AudioBufferRequest& request,AudioBufferId& out)override{
        creates.push_back(id);
        if(mode==Mode::lock_retention){memory.assign(request.bytes,0xcd);out={0x1111};return 0;}
        // Model an owner mutation at a synchronous external boundary. Failed
        // individual loads must still permit subsequent source catalog work.
        if(creates.size()==1){
            if(mode==Mode::catalog_mode)*enabled=0;
            if(mode==Mode::catalog_device)*device={0xbbbb};
        }
        return 1;
    }
    AudioResult query_spatial_buffer(AudioBufferId,AudioSpatialId&)override{throw std::runtime_error("unexpected QI");}
    AudioResult lock_buffer(AudioBufferId id,std::uint32_t offset,std::uint32_t count,AudioBufferMemory& out,std::uint32_t flags)override{
        require(mode==Mode::lock_retention&&id.value==0x1111&&offset==0&&flags==2,"unexpected Lock arguments");
        locked_bytes=count;out.first=memory;out.second={};
        sample->buffer={0x2222};fields->bytes=0;
        return 0;
    }
    AudioResult unlock_buffer(AudioBufferId id,std::span<std::uint8_t> first,std::span<std::uint8_t> second)override{
        require(first.data()==memory.data()&&second.empty(),"unexpected Unlock memory");
        unlocked=id;unlock_bytes=first.size();return 0;
    }
    AudioResult capabilities(AudioDeviceId,std::span<std::uint32_t,24>)override{throw std::runtime_error("unexpected caps");}
    AudioResult primary_format(AudioPrimaryId,AudioWaveFormat&,std::uint32_t,std::uint32_t&)override{throw std::runtime_error("unexpected format read");}
    AudioResult set_primary_format(AudioPrimaryId,const AudioWaveFormat&)override{throw std::runtime_error("unexpected format write");}
};
bool lock_red(const AssetStore& assets){
    AudioSample sample;AudioSampleLoadFields fields;
    Boundary boundary(Boundary::Mode::lock_retention);boundary.sample=&sample;boundary.fields=&fields;
    const auto resource=audio_sample_definitions[0].resource;
    const auto wave=assets.resource(resource);std::uint32_t payload{};std::memcpy(&payload,wave.data()+40,4);
    require(payload>0&&wave.size()>=44ull+payload,"fixture WAV bounds");
    require(load_audio_sample(sample,fields,{0xaaaa},boundary,assets,resource,0,2),"unexpected loader result");
    const bool copied=std::equal(wave.begin()+44,wave.begin()+44+payload,boundary.memory.begin());
    const bool match=boundary.unlocked.value==0x1111&&boundary.unlock_bytes==payload&&copied;
    std::cout<<"461ECC/461EDD retained buffer actual="<<std::hex<<boundary.unlocked.value<<" expected=1111"<<std::dec
        <<";461ED0/461EE9 retained count actual="<<boundary.unlock_bytes<<" expected="<<payload
        <<";copy matches="<<copied<<" expected=1\n";
    require(sample.buffer.value==0x2222&&fields.bytes==0,"callback owner writes were lost");
    return match;
}
bool catalog_red(const AssetStore& assets,bool mutate_mode){
    AudioDriverState driver;AudioSampleLoadStorage storage;std::array<std::uint32_t,72> types{};
    types[1]=2;types[2]=2;std::uint32_t enabled=1;AudioDeviceId device{0xaaaa};
    Boundary boundary(mutate_mode?Boundary::Mode::catalog_mode:Boundary::Mode::catalog_device);
    boundary.enabled=&enabled;boundary.device=&device;
    require(load_audio_samples(driver,storage,types,device,boundary,assets,enabled),"catalog did not ignore failed sample loads");
    require(boundary.creates.size()>=2,"missing second CreateSoundBuffer");
    if(mutate_mode){
        // Source reload 461F86 sees zero from the previous boundary. It sets
        // remaining descriptor types to1 at461F8F; second pass loads slot2 as2D.
        const bool match=types[2]==1&&driver.samples[2].type==1;
        std::cout<<"461F86 fresh spatial global: descriptor2 actual="<<types[2]<<" expected=1;sample2.type actual="
            <<driver.samples[2].type<<" expected=1\n";return match;
    }
    // Separate source loads reach 461E92 afresh; the device is not a catalog local.
    const bool match=boundary.creates[1].value==0xbbbb;
    std::cout<<"461E92 fresh device global: second CreateSoundBuffer actual="<<std::hex<<boundary.creates[1].value
        <<" expected=bbbb\n"<<std::dec;return match;
}
}
int main(int argc,char** argv){try{
    require(argc==3,"expected case and retail root");AssetStore assets(argv[2]);const std::string_view mode=argv[1];
    const bool matched=mode=="lock"?lock_red(assets):mode=="mode"?catalog_red(assets,true):
        mode=="device"?catalog_red(assets,false):throw std::runtime_error("unknown case");
    if(matched){std::cout<<"SOURCE_CONTRACT_MATCH\n";return 0;}
    std::cout<<"EXPECTED_RED_CURRENT_SAMPLE_WRAPPER\n";return 1;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}}
