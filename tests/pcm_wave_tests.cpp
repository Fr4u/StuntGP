#include "sgn/platform_audio.hpp"
#include "sgn/retail_audio_catalog.hpp"
#include <cstring>
#include <iostream>
#include <set>

namespace {
using namespace sgn;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void put(Bytes& bytes,std::size_t at,unsigned value){std::memcpy(bytes.data()+at,&value,4);}
template<class F>void rejects(F action){bool rejected=false;try{action();}catch(const FormatError&){rejected=true;}require(rejected,"invalid WAVE accepted");}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected retail data root");AssetStore assets(argv[1]);unsigned decoded=0;std::uint64_t samples=0;std::set<unsigned> rates;
    for(const auto& definition:sgn::retail::audio_sample_definitions){const auto bytes=assets.resource(definition.resource);const auto wave=decode_pcm_wave(bytes);
        require(wave.channels==1&&wave.bits_per_sample==16&&wave.block_align==2,"retail audio format changed");unsigned authored_rate;std::memcpy(&authored_rate,bytes.data()+24,4);
        require(wave.sample_rate==authored_rate,"authored sample rate rounded or replaced");rates.insert(wave.sample_rate);samples+=wave.samples.size()/wave.block_align;++decoded;}
    require(decoded==71&&rates==std::set<unsigned>{11025,11050,22050},"source audio catalog coverage");
    const auto original=assets.resource(sgn::retail::audio_sample_definitions[0].resource);Bytes valid(original.begin(),original.end());
    unsigned negative=0;
    for(std::size_t count:std::array<std::size_t,6>{0,1,8,11,20,valid.size()-1}){rejects([&]{decode_pcm_wave(std::span(valid).first(count));});++negative;}
    for(const auto [offset,value]:std::array<std::pair<unsigned,unsigned>,7>{{{0,0},{8,0},{4,0xffffffff},{16,0xffffffff},{20,0},{24,0},{28,1}}}){
        auto broken=valid;put(broken,offset,value);rejects([&]{decode_pcm_wave(broken);});++negative;}
    auto trailing=valid;trailing.push_back(0);rejects([&]{decode_pcm_wave(trailing);});++negative;
    // Chunk walking must accept optional odd-sized chunks with RIFF padding.
    auto unknown=valid;const std::array<std::uint8_t,10> chunk{'J','U','N','K',1,0,0,0,7,0};unknown.insert(unknown.begin()+12,chunk.begin(),chunk.end());put(unknown,4,static_cast<unsigned>(unknown.size()-8));
    require(decode_pcm_wave(unknown).samples==decode_pcm_wave(valid).samples,"optional padded WAVE chunk altered samples");
    std::cout<<"catalog="<<decoded<<" pcm_samples="<<samples<<" exact_sample_rates=3 negative_cases="<<negative<<" errors=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
