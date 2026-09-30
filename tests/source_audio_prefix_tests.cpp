#include "sgn/retail_audio_initialize.hpp"
#include "source_audio_prefix_expected.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace sgn::retail;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void check(std::uint32_t input){
    AudioInitializationState audio;AudioDriverState driver;AudioMaintenanceState maintenance;
    LevelAudioState level{75,60,input,114,153,97};
    audio.device={123};audio.primary={456};audio.enumeration.records.fill(0xa5);
    audio.enumeration.cursor_word=23;audio.enumeration.count=99;audio.enumeration.adjacent_word=0x12345678;
    audio.descriptor_types.fill(0xcccccccc);audio.spatial_capability=7;audio.spatial_enabled=8;
    audio.saved_spatial_mode=19;audio.requested_rate=44100;audio.sample_fields.samples[127]={11,22,33,44};
    driver.initialized=1;driver.spatial_mode=31;driver.range=71;driver.samples[103].buffer={701};
    maintenance.listener={999};maintenance.music.buffer={333};maintenance.music.gain_step=47;
    require(initialize_audio_prefix_462090(audio,driver,maintenance,level)==(input!=0),"source input gate differs");
    require(audio.descriptor_types==fixture::prefix_descriptor_types,"72 descriptor types differ from independently pinned PE words");
    require(std::all_of(audio.enumeration.records.begin(),audio.enumeration.records.end(),[](auto b){return b==0;})&&
        audio.enumeration.cursor_word==0x62f128&&audio.enumeration.count==0&&audio.enumeration.adjacent_word==0x12345678,
        "enumeration reset extent differs");
    require(audio.spatial_enabled==1&&audio.spatial_capability==1&&driver.initialized==0&&
        audio.device.value==0&&audio.primary.value==0&&maintenance.listener.value==0&&level.output_b==256,
        "prefix mutations missing, including on disabled branch");
    require(audio.saved_spatial_mode==19&&audio.requested_rate==44100&&audio.sample_fields.samples[127].ready==11&&
        audio.sample_fields.samples[127].bytes==22&&audio.sample_fields.samples[127].bits==33&&audio.sample_fields.samples[127].channels==44&&
        driver.spatial_mode==31&&driver.range==71&&driver.samples[103].buffer.value==701&&
        maintenance.music.buffer.value==333&&maintenance.music.gain_step==47&&level.scale==75&&level.input_a==60&&
        level.input_b==input&&level.output_a==114&&level.output_b_copy==97,"prefix changed unrelated or later source fields");
}
}
int main(){try{
    for(const auto input:{0u,1u,80u,0x80000000u,0xffffffffu})check(input);
    std::cout<<"SOURCE_AUDIO_PREFIX_PASS 5 input words;72 PE types;1152-byte extent;preserved neighbors\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
