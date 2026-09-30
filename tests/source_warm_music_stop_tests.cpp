#include "source_music_stop_fixture.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include <atomic>
#include <thread>

namespace {
unsigned checks{},received{},skipped{},not_ready{},timer_stops{},buffer_stops{},forwarded{};
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
Record state_image(const Record& seed,RaceSessionState& state){
    auto out=seed;AudioPrimaryId primary{word(seed,0x214)};AudioListenerId listener{word(seed,0x260)};
    transfer(out,state.audio_stream,state.audio_maintenance.music,state.audio_level,primary,listener,false);return out;
}
void source_owners_absent(RaceSessionPlatform& platform){
    const auto h=platform.host_readiness();check(!h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"stop prepared a host backend");
    bool absent=false;try{(void)platform.source_audio_owner();}catch(const FormatError& e){absent=std::string_view(e.what())=="source audio owner has not been constructed";}
    check(absent,"stop constructed an audio owner");
    check(!platform.source_allocations().initialized()&&platform.source_allocations().live_count()==0,"stop initialized source allocations");
}
void load_state(Record data,RaceSessionState& state){
    AudioPrimaryId primary;AudioListenerId listener;
    transfer(data,state.audio_stream,state.audio_maintenance.music,state.audio_level,primary,listener,true);
}
void run(const std::filesystem::path& root,const char* path){
    auto platform=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    RaceSession session(SourceColdAttachmentOnly{},platform);auto& s=session.state();
    check(session.shared_platform()==platform&&&s==&platform->state(),"source Session world is not retained platform world");
    s.scene_mode=2;s.normal_finish.menu_requested=0;s.options.selection_phase=2;s.pause_flag=0;
    source_owners_absent(*platform);
    std::ifstream in(path,std::ios::binary);std::array<char,8> magic{};read(in,magic);
    check(std::string_view(magic.data(),magic.size())=="SGNAST01","bad existing startup corpus");
    unsigned count{};read(in,count);check(count==1792,"wrong existing corpus extent");
    for(unsigned n=0;n<count;++n){
        unsigned op{},event_count{};Record before,after;read(in,op);read(in,before);read(in,after);read(in,event_count);
        check(op<=6&&event_count<=32,"invalid existing corpus record");std::vector<Event> events(event_count);for(auto& e:events)read(in,e);
        if(op!=4)continue;++received;
        load_state(before,s);const auto original=state_image(before,s);
        const bool entered=s.audio_stream.playback_active!=0&&s.audio_level.output_a!=0;
        const bool ready=s.audio_stream.allocated!=0&&(s.audio_stream.flags&0x4000u)!=0;
        const auto result=stop_source_music_45f9d0(*platform);Record expected=before;
        if(!entered){
            ++skipped;check(result.status==SourceMusicStopStatus::completed&&result.blocked_address==0&&result.required_identity==0,"45F9D0 guard reached native boundary");
        }else if(!ready){
            ++not_ready;check(result.status==SourceMusicStopStatus::completed,"not-ready460330 was mistaken for missing adapter");
            // The recorded complete460330 result, plus the literal45F9E7 store.
            expected=after;expected[0x264/4]=before[0x264/4];expected[0x220/4]=0;
            check(expected[0x18c/4]==0x887800aau,"original not-ready corpus result drift");
        }else{
            ++timer_stops;check(s.audio_stream.timer!=0,"saved corpus no longer has its expected nonzero ready timer");
            expected[0x38/4]|=0x8000u;
            check(result.status==SourceMusicStopStatus::timer_required&&result.blocked_address==0x460365&&result.required_identity==word(before,0x50),
                "first ready source timer boundary is wrong");
        }
        check(state_image(before,s)==expected,"retained45F9D0 prefix state differs from source evidence");
        check(s.scene_mode==2&&s.normal_finish.menu_requested==0&&s.options.selection_phase==2&&s.pause_flag==0,"stop child changed caller scene stores");
        check(original==before,"receiving codec did not preserve unmodeled original words");

        // Check the sole45F9D0 body through its narrower view against the
        // original460330 result plus its caller's literal final store.
        AudioStreamState stream;AudioMusicState music;LevelAudioState level;AudioPrimaryId primary;AudioListenerId listener;
        auto projected=before;transfer(projected,stream,music,level,primary,listener,true);Device device(before);
        stop_audio_music(AudioMusicStopWorld{stream,music,level,device,device});
        transfer(projected,stream,music,level,primary,listener,false);
        Record complete=before;
        if(entered){complete=after;complete[0x264/4]=before[0x264/4];complete[0x220/4]=0;}
        check(projected==complete&&device.events==(entered?events:std::vector<Event>{}),"one narrowed45F9D0 body differs from recorded460330 plus literal caller");
        ++forwarded;

        if(entered&&ready){
            // Supplementary explicit timer-zero input; no saved corpus is edited.
            // Original460362 branches straight to460379; only the same atomic
            //flag write precedes it. The buffer token remains pointer-width.
            load_state(before,s);s.audio_stream.timer=0;s.audio_maintenance.music.buffer={0xfedcba9876543210ull};
            const auto current=state_image(before,s);auto wanted=current;wanted[0x38/4]|=0x8000u;
            const auto zero=stop_source_music_45f9d0(*platform);++buffer_stops;
            check(zero.status==SourceMusicStopStatus::buffer_stop_required&&zero.blocked_address==0x460379&&zero.required_identity==0xfedcba9876543210ull,
                "timer-zero source buffer stop identity/site is wrong");
            check(state_image(before,s)==wanted&&s.audio_maintenance.music.buffer.value==0xfedcba9876543210ull,
                "buffer stop cleared counters or truncated/replaced source identity");
        }
    }
    check(in.peek()==std::char_traits<char>::eof(),"trailing startup corpus");
    check(received==256&&skipped&&not_ready&&timer_stops&&buffer_stops&&forwarded==received,"missing guarded/not-ready/native boundaries");
    source_owners_absent(*platform);
    auto& owner=platform->prepare_source_audio_owner();const auto* retained=&owner;
    s.audio_stream.playback_active=1;s.audio_level.output_a=1;s.audio_stream.allocated=1;s.audio_stream.flags=0x4000;s.audio_stream.timer=77;
    Record seed{};const auto before=state_image(seed,s);std::atomic<bool> refused{false};
    std::thread foreign([&]{try{(void)stop_source_music_45f9d0(*platform);}catch(const std::logic_error&){refused=true;}});foreign.join();
    check(refused&&state_image(seed,s)==before,"foreign thread executed source flag exchange");
    RaceSessionPlatform other(SourceColdConstructionOnly{},root,0.0f);const auto other_before=state_image(seed,other.state());
    const auto selected=stop_source_music_45f9d0(*platform);
    check(selected.status==SourceMusicStopStatus::timer_required&&selected.required_identity==77,"retained-owner timer branch drift");
    check(&platform->source_audio_owner()==retained&&!owner.host_backend_ready()&&owner.source_device_identity()==0,"stop replaced/prepared deferred native owner");
    check(state_image(seed,other.state())==other_before,"stop mutated another world");
    // Allocated receiving bits are not claimed as actual native resources;
    // no stream owner was created, so no native cleanup is triggered.
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected NEW retail root and immutable startup corpus");run(argv[1],argv[2]);
    std::cout<<"PASS source stop: corpus="<<received<<" guard="<<skipped<<" not_ready="<<not_ready<<" timer="<<timer_stops
        <<" buffer_zero_timer="<<buffer_stops<<" projected="<<forwarded<<" checks="<<checks<<" no_native_backend=1\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
