#include "audio_oracle_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include <atomic>
#include <cstddef>
#include <thread>
#include <type_traits>

namespace {
unsigned checks{},received{},guarded{},completed{},boundaries{},full_width{},empty_cases{},scalar_cases{};
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
template<class T> auto image(const T& value){
    static_assert(std::is_trivially_copyable_v<T>);
    std::array<std::byte,sizeof(T)> result{};std::memcpy(result.data(),&value,sizeof(T));return result;
}
auto driver_with_count(const AudioDriverState& state,std::uint32_t count){
    auto result=image(state);
    static_assert(std::is_standard_layout_v<AudioDriverState>);
    std::memcpy(result.data()+offsetof(AudioDriverState,playing_count),&count,sizeof(count));return result;
}
void no_source_owners(RaceSessionPlatform& platform){
    const auto h=platform.host_readiness();
    check(!h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"sample stop prepared a host backend");
    bool absent=false;try{(void)platform.source_audio_owner();}catch(const sgn::FormatError& e){
        absent=std::string_view(e.what())=="source audio owner has not been constructed";
    }
    check(absent,"sample stop constructed a native owner");
    check(!platform.source_allocations().initialized()&&platform.source_allocations().live_count()==0,
        "sample stop initialized source allocations");
}
void load(Record data,RaceSessionState& state){
    transfer(data,state.audio_driver,state.audio_level,state.controls,true);state.split_setting=data[8];
}
Record record(const Record& seed,RaceSessionState& state){
    auto data=seed;transfer(data,state.audio_driver,state.audio_level,state.controls,false);return data;
}
void caller_unchanged(const RaceSessionState& s){
    check(s.scene_mode==2&&s.normal_finish.menu_requested==0&&s.options.selection_phase==2&&s.pause_flag==0,
        "sample child modified preceding caller scene stores");
}
void run(const std::filesystem::path& root,const char* corpus){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    RaceSession session(SourceColdAttachmentOnly{},platform);auto& s=session.state();
    check(session.shared_platform()==platform&&&s==&platform->state(),"source Session world is not retained");
    s.scene_mode=2;s.normal_finish.menu_requested=0;s.options.selection_phase=2;s.pause_flag=0;
    no_source_owners(*platform);
    std::ifstream in(corpus,std::ios::binary);std::array<char,8> magic{};read(in,magic);
    check(std::string_view(magic.data(),magic.size())=="SGNAUD01","wrong immutable driver corpus");
    unsigned count{};read(in,count);check(count==4096,"wrong driver corpus extent");
    for(unsigned n=0;n<count;++n){
        unsigned op{},event_count{};Record before{},after{};read(in,op);read(in,before);read(in,after);read(in,event_count);
        check(op<=9&&event_count<=256,"invalid driver corpus record");
        std::vector<Event> events(event_count);for(auto& event:events)read(in,event);
        if(op!=3)continue;++received;
        load(before,s);check(record(before,s)==before,"driver codec changed an input word");
        const auto level_before=image(s.audio_level);const auto stream_before=image(s.audio_stream);
        const auto music_before=image(s.audio_maintenance.music);const auto controls_before=image(s.controls);
        unsigned first=0;while(first<128&&before[32+first*4]==0)++first;
        const bool blocked=before[0]!=0&&first<128;
        const auto expected_count=before[6]-(before[0]!=0?first:0u);
        const auto driver_expected=driver_with_count(s.audio_driver,expected_count);
        const auto result=stop_source_samples_460dd0(*platform);
        if(blocked){
            ++boundaries;
            check(result.status==SourceSampleStopStatus::buffer_stop_required&&result.blocked_address==0x460d79&&
                result.required_buffer.value==before[32+first*4],"wrong first source sample Stop boundary");
            check(!events.empty()&&events.front()[0]==8&&events.front()[1]==result.required_buffer.value,
                "required identity differs from first immutable original Stop event");
            auto expected=before;expected[6]=expected_count;
            check(record(before,s)==expected,"sample boundary lost prior empty-slot decrements or ran later writes");
        }else{
            ++completed;if(before[0]==0)++guarded;
            check(result.status==SourceSampleStopStatus::completed&&result.blocked_address==0&&result.required_buffer.value==0,
                "completed sample loop was mistaken for an unresolved native operation");
            check(events.empty()&&record(before,s)==after,"completed loop differs from immutable original result");
        }
        check(image(s.audio_driver)==driver_expected,"driver fields changed outside source count prefix");
        check(image(s.audio_level)==level_before&&image(s.audio_stream)==stream_before&&
            image(s.audio_maintenance.music)==music_before&&image(s.controls)==controls_before&&s.split_setting==before[8],
            "sample stop changed another retained owner");
        caller_unchanged(s);
    }
    check(in.peek()==std::char_traits<char>::eof(),"trailing driver corpus data");
    check(received==332&&guarded==33&&completed+boundaries==received,"stop-all corpus coverage drift");
    // Explicit supplemental inputs test the pointer-width host identity at
    // every possible first occupied source slot. They are not original runs.
    for(unsigned first=0;first<128;++first){
        s.audio_driver={};s.audio_driver.initialized=2;s.audio_driver.playing_count=10;
        const std::uint64_t identity=0xfedcba9800000000ull+first;
        s.audio_driver.samples[first].buffer={identity};
        s.audio_driver.samples[first].spatial={0xabcdef9876543210ull};
        if(first<127)s.audio_driver.samples[first+1].buffer={identity+0x10000};
        const auto expected=driver_with_count(s.audio_driver,10u-first);
        const auto level=image(s.audio_level);const auto stopped=stop_source_samples_460dd0(*platform);
        check(stopped.status==SourceSampleStopStatus::buffer_stop_required&&stopped.blocked_address==0x460d79&&
            stopped.required_buffer.value==identity,"supplemental source slot/full-width token drift");
        check(image(s.audio_driver)==expected&&image(s.audio_level)==level,"current/later slot or reset executed past missing Stop");
        ++full_width;
    }
    for(const std::uint32_t initialized:{1u,2u,0xffffffffu}){
        for(const std::uint32_t initial:{0u,1u,127u,128u,0x7fffffffu,0x80000000u,0xffffffffu}){
            s.audio_driver={};s.audio_driver.initialized=initialized;s.audio_driver.playing_count=initial;
            const auto expected=driver_with_count(s.audio_driver,initial-128u);
            const auto result=stop_source_samples_460dd0(*platform);
            check(result.status==SourceSampleStopStatus::completed&&result.blocked_address==0&&result.required_buffer.value==0&&
                image(s.audio_driver)==expected,"128 null slots did not decrement modulo2^32");++empty_cases;
        }
    }
    // Four distinct unrelated level words plus driver initialization variations
    // distinguish exact460C90 from option recalculation or gain advancement.
    for(unsigned i=0;i<256;++i){
        s.audio_level={0x12345678u+i,0x98765432u-i,0xa5a5a5a5u^i,0x76543210u+i,0xdeadbeefu-i,i};
        s.audio_driver.initialized=i%3;const auto driver=image(s.audio_driver);
        auto expected=s.audio_level;expected.output_b=255;expected.output_b_copy=255;
        reset_source_sound_output_460c90(*platform);
        check(image(s.audio_level)==image(expected)&&image(s.audio_driver)==driver,"460C90 changed more than two output words");
        caller_unchanged(s);++scalar_cases;
    }
    no_source_owners(*platform);
    auto& owner=platform->prepare_source_audio_owner();const auto* retained=&owner;
    s.audio_driver={};s.audio_driver.initialized=1;s.audio_driver.playing_count=3;
    s.audio_driver.samples[3].buffer={0x8765432100000003ull};
    s.audio_level.output_b=17;s.audio_level.output_b_copy=19;
    const auto original_driver=image(s.audio_driver);const auto original_level=image(s.audio_level);
    std::atomic<unsigned> refused{0};
    std::thread foreign([&]{
        try{(void)stop_source_samples_460dd0(*platform);}catch(const std::logic_error&){++refused;}
        try{reset_source_sound_output_460c90(*platform);}catch(const std::logic_error&){++refused;}
    });foreign.join();
    check(refused==2&&image(s.audio_driver)==original_driver&&image(s.audio_level)==original_level,
        "foreign thread mutated source count/output before rejection");
    RaceSessionPlatform other(SourceColdConstructionOnly{},root,0.0f);
    const auto other_driver=image(other.state().audio_driver);const auto other_level=image(other.state().audio_level);
    const auto stopped=stop_source_samples_460dd0(*platform);
    check(stopped.status==SourceSampleStopStatus::buffer_stop_required&&stopped.required_buffer.value==0x8765432100000003ull&&
        s.audio_driver.playing_count==0,"deferred owner altered the first Stop boundary");
    reset_source_sound_output_460c90(*platform); // Explicit isolated scalar test, not a claim of caller progression.
    check(&platform->source_audio_owner()==retained&&!owner.host_backend_ready()&&owner.source_device_identity()==0,
        "source sample/reset helper replaced or prepared deferred owner");
    check(image(other.state().audio_driver)==other_driver&&image(other.state().audio_level)==other_level,
        "source sample/reset helper mutated another platform");
    // Supplemental state does not represent native allocation. No stream owner
    // or native source sample registry was populated by these CPU inputs.
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected NEW retail root and immutable audio-driver corpus");run(argv[1],argv[2]);
    std::cout<<"PASS source samples: corpus="<<received<<" guard="<<guarded<<" completed="<<completed<<" boundaries="<<boundaries
        <<" full_width="<<full_width<<" empty="<<empty_cases<<" scalar="<<scalar_cases<<" checks="<<checks<<" no_native_backend=1\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
