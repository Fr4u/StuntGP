#include "audio_initialize_state.hpp"
#include <array>
#include <tuple>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::test::audio_initialize;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
unsigned random_word(unsigned& state){state=state*1664525u+1013904223u;return state;}
unsigned scale_from_division(unsigned scale,unsigned input){
    //Independent arithmetic expectation: PE low32 products, then unsigned /10000.
    //Production460BC0 uses multiply-high by its reciprocal instead.
    const auto first=static_cast<unsigned>(std::uint64_t(scale)*input);
    const auto wrapped=static_cast<unsigned>(std::uint64_t(first)*255u);
    return wrapped/10000u;
}
auto extra(const AudioStreamState& s){
    return std::tuple{s.filename_tail,s.timer_capabilities.minimum(),s.timer_capabilities.maximum(),s.format.tag,s.format.channels,
        s.format.samples_per_second,s.format.bytes_per_second,s.format.block_align,s.format.bits_per_sample,
        s.format.extra_bytes,s.chunk_id,s.chunk_size,s.chunk_type,s.chunk_flags,s.stop_threshold,s.status};
}
}
int main(){try{
    static_assert(sizeof(unsigned)==4);
    constexpr std::array<unsigned,22> cleared_dwords{0x62df0c,0x62df1c,0x62df20,0x62df14,0x62df24,
        0x62df28,0x62df2c,0x62df30,0x62df34,0x62e084,0x62df38,0x62e090,0x62e098,0x62e09c,
        0x62e0a0,0x62df3c,0x62df40,0x62df50,0x62e06c,0x62e088,0x62e08c,0x62df08};
    constexpr std::array<unsigned,7> music_clears{0x62e10c,0x62e0fc,0x62e110,0x62e100,0x62e104,0x62e108,0x62f5ac};
    constexpr std::array<unsigned,6> inputs{0u,1u,100u,255u,65535u,0xffffffffu};
    unsigned cases=0,reset_cases=0,no_write_cases=0,seed=0x462187u;std::uint64_t words=0;
    for(const bool returned:{false,true})for(const unsigned playback:{0u,1u,0x80000000u})
    for(const unsigned old_output:{0u,1u,0xdeadbeefu})for(const unsigned scale:{0u,255u,0x80000000u,0xffffffffu})
    for(std::size_t input=0;input<inputs.size();++input)for(const unsigned enabled:{0u,1u,7u,0xffffffffu}){
        Record actual;for(auto& word:actual)word=random_word(seed);
        actual[at(0x62f5ac)]=playback;actual[at(0x62e0f8)]=old_output;actual[at(0x489790)]=scale;
        actual[at(0x62e0a8)]=inputs[input];actual[at(0x62e0f0)]=inputs[inputs.size()-1-input];
        actual[at(0x4897b0)]=enabled;
        //Ensure the two byte clears cannot accidentally pass as whole-word clears.
        actual[at(0x62df18)]=0x87654321u;actual[at(0x62df54)]=0xfedcba98u;
        auto expected=actual;
        AudioDriverState driver;AudioInitializationState init;AudioMaintenanceState maintenance;
        AudioStreamState stream;LevelAudioState level;LevelOverrideOptions options;
        transfer(actual,driver,init,maintenance,stream,level,options,true);
        stream.filename_tail.fill(0xadu);stream.timer_capabilities.minimum()=23;stream.timer_capabilities.maximum()=941;
        stream.format={1,3,192000,1152000,6,16,19};
        stream.chunk_id=0xabcdef01;stream.chunk_size=0x23456789;stream.chunk_type=0xdead1234;
        stream.chunk_flags=0x98765432;stream.stop_threshold=17;stream.status=31;
        const auto preserved=extra(stream);
        if(returned){
            expected[at(0x62e0ac)]=1;
            if(playback==0&&old_output!=0){
                for(auto address:cleared_dwords)expected[at(address)]=0;
                expected[at(0x62df18)]&=0xffffff00u;expected[at(0x62df54)]&=0xffffff00u;
                expected[at(0x62e0f8)]=0x66;
                for(auto address:music_clears)expected[at(address)]=0;
                ++reset_cases;
            }
            expected[0x1a80/4]=inputs[input];expected[0x1a84/4]=inputs[inputs.size()-1-input];
            expected[at(0x62e0f8)]=scale_from_division(scale,inputs[input]);
            expected[at(0x489798)]=scale_from_division(scale,inputs[inputs.size()-1-input]);
            expected[at(0x48979c)]=expected[at(0x489798)];expected[at(0x4897b4)]=enabled;
        }else ++no_write_cases;
        const auto result=complete_audio_initialization_tail_462187(returned,driver,stream,maintenance.music,level,options,init.spatial_enabled);
        require(result==returned,"tail return differs from source catalog test");
        require(extra(stream)==preserved,"tail reset an unlisted extended stream field");
        transfer(actual,driver,init,maintenance,stream,level,options,false);
        for(std::size_t i=0;i<actual.size();++i){
            ++words;if(actual[i]!=expected[i]){
                std::cerr<<"case "<<cases<<" offset "<<std::hex<<i*4<<" actual "<<actual[i]<<" expected "<<expected[i]<<'\n';
                throw std::runtime_error("source tail word mismatch");
            }
        }
        ++cases;
    }
    std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"compared_words\":"<<words<<",\"false_no_write\":"<<no_write_cases
        <<",\"music_reset_cases\":"<<reset_cases<<",\"native_calls\":0,\"extended_state_preserved\":true}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
