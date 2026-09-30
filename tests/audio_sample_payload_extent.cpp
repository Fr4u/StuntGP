#include "sgn/retail_audio_catalog_startup.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace sgn;
using namespace sgn::retail;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Boundary final:AudioCreationDevice {
    std::string_view mode;
    AudioSample& sample;
    AudioSampleLoadFields& fields;
    std::uint32_t requested;
    Bytes memory;
    unsigned creates{},locks{},unlocks{};
    Boundary(std::string_view value,AudioSample& s,AudioSampleLoadFields& f,std::uint32_t count):mode(value),sample(s),fields(f),requested(count){}
    AudioResult create_buffer(AudioDeviceId device,const AudioBufferRequest&,AudioBufferId& out)override{
        require(device.value==0xaaaa,"source create identity");++creates;
        out={0x1111};fields.bytes=requested;return 0;
    }
    AudioResult lock_buffer(AudioBufferId id,std::uint32_t offset,std::uint32_t count,AudioBufferMemory& out,std::uint32_t flags)override{
        require(id.value==0x1111&&offset==0&&count==requested&&flags==2,"retained source Lock arguments");++locks;
        memory.assign(count,0xcd);
        out.first=std::span(memory).first(mode=="short_first"?count-1:count);out.second={};
        sample.buffer={0x2222};fields.bytes=0;
        return mode=="failed_lock"?AudioResult{5}:AudioResult{0};
    }
    AudioResult unlock_buffer(AudioBufferId id,std::span<std::uint8_t> first,std::span<std::uint8_t> second)override{
        require(id.value==0x1111&&first.data()==memory.data()&&first.size()==requested&&second.empty(),"retained source Unlock arguments");++unlocks;return 0;
    }
    AudioResult query_spatial_buffer(AudioBufferId,AudioSpatialId&)override{throw std::runtime_error("unexpected QI");}
    AudioResult capabilities(AudioDeviceId,std::span<std::uint32_t,24>)override{throw std::runtime_error("unexpected caps");}
    AudioResult primary_format(AudioPrimaryId,AudioWaveFormat&,std::uint32_t,std::uint32_t&)override{throw std::runtime_error("unexpected format read");}
    AudioResult set_primary_format(AudioPrimaryId,const AudioWaveFormat&)override{throw std::runtime_error("unexpected format write");}
};
}
int main(int argc,char** argv)try{
    require(argc==3,"expected case and retail root");const std::string_view mode=argv[1];
    require(mode=="equal"||mode=="over"||mode=="failed_lock"||mode=="short_first","unknown extent case");
    AssetStore assets(argv[2]);const auto resource=audio_sample_definitions[0].resource;
    const auto wave=assets.resource(resource);
    require(wave.size()>44&&wave.size()-44<std::numeric_limits<std::uint32_t>::max(),"fixture extent");
    const auto available=static_cast<std::uint32_t>(wave.size()-44);
    std::uint32_t header_payload{};std::memcpy(&header_payload,wave.data()+40,4);
    require(header_payload>0&&header_payload<=available,"fixture header payload");
    const auto requested=available+(mode=="over"||mode=="failed_lock"?1u:0u);
    AudioSample sample;AudioSampleLoadFields fields;
    Boundary boundary(mode,sample,fields,requested);
    bool returned=false,rejected=false;
    try{returned=load_audio_sample(sample,fields,{0xaaaa},boundary,assets,resource,0,2);}
    catch(const std::out_of_range& error){
        const std::string_view expected=mode=="over"?"source audio payload exceeds retained WAV storage":"source audio lock first region too small";
        require(error.what()==expected,"unexpected extent rejection");rejected=true;
    }
    require(boundary.creates==1&&boundary.locks==1,"source prefix must reach Create and Lock exactly once");
    require(sample.buffer.value==0x2222&&fields.bytes==0&&fields.ready==1,"callback state or published ready prefix lost");
    if(mode=="equal"){
        require(returned&&!rejected&&boundary.unlocks==1,"valid complete backing extent must load");
        require(std::equal(boundary.memory.begin(),boundary.memory.end(),wave.begin()+44),"complete payload including bounded padding");
    }else{
        require(!returned&&boundary.unlocks==0,"failure must not copy or Unlock");
        require(rejected==(mode!="failed_lock"),"failed Lock must return before host extent checks");
        require(std::all_of(boundary.memory.begin(),boundary.memory.end(),[](auto byte){return byte==0xcd;}),"memory changed before rejection");
    }
    std::cout<<"{\"passed\":true,\"case\":\""<<mode<<"\",\"header_payload\":"<<header_payload
        <<",\"available_after_header\":"<<available<<",\"requested\":"<<requested
        <<",\"creates\":"<<boundary.creates<<",\"locks\":"<<boundary.locks<<",\"unlocks\":"<<boundary.unlocks
        <<",\"original_execution_oracle\":false}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
