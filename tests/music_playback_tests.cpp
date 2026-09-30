#include "sgn/platform_audio.hpp"
#include <windows.h>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void pump(WindowsAudioStreamPlatform& platform,unsigned duration){
    const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(duration);
    while(std::chrono::steady_clock::now()<until){
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        platform.check_health();std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
struct Fixture {
    WindowsAudioDevice device{0};
    WindowsAudioStreamPlatform platform;
    AudioDeviceId device_id;
    AudioDriverState driver;
    LevelAudioState level;
    contact::player::Controls controls;
    unsigned split{};
    AudioMusicState music;
    AudioStreamState stream;
    AudioMusicTrack track;
    explicit Fixture(const std::filesystem::path& root):platform(device,root){
        require(device.create_default_device(device_id)==0,"create default music device");
        driver.initialized=1;level.output_a=102;
    }
    AudioStreamPlaybackWorld world(){return {{driver,device,level,controls,split},music,stream,device_id,device,device,platform,device};}
    void close(){close_audio_stream(stream,music,device,platform);}
    ~Fixture(){try{close();}catch(...){platform.cancel_pending_callbacks();}}
};
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("expected imported retail root and synthetic failure fixture root");
    Fixture f(argv[1]);unsigned checks=0,opened=0;std::uint64_t played22050=0,played32000=0,publications=0;
    auto check=[&](bool value,const char* message){require(value,message);++checks;};
    // Disabled and zero-selected branches must not construct a voice or timer.
    f.track.selected=2;f.driver.initialized=0;start_audio_music(f.world(),f.track,0,1);
    check(!f.stream.allocated&&!f.music.buffer.value,"disabled driver started music");
    f.driver.initialized=1;f.level.output_a=0;start_audio_music(f.world(),f.track,0,1);
    check(!f.stream.allocated,"disabled music started stream");f.level.output_a=102;
    f.track.selected=0;start_audio_music(f.world(),f.track,0,1);check(!f.stream.playback_active,"zero track started stream");
    f.track.selected=2;unsigned delay=2;advance_audio_music_delay(f.world(),f.track,delay);
    check(delay==1&&!f.stream.allocated,"delay started a tick early");
    advance_audio_music_delay(f.world(),f.track,delay);
    check(delay==0&&f.stream.allocated&&f.stream.playback_active&&f.music.buffer.value,"actual441F70 music open failed");
    check(f.stream.field_e10c==2&&f.stream.flags==0x4025&&f.platform.live_files()==1&&f.platform.live_timers()==1,"startup owner/flags mismatch");
    const auto first_id=f.music.buffer;auto world=f.world();audio_advance_music_gain(world.audio,f.music);audio_advance_music_gain(world.audio,f.music);
    check(f.music.gain==255&&f.music.gain_step==0&&f.music.requested_volume==255,"source gain maintenance not connected");
    pump(f.platform,1850);auto info=f.device.inspect(f.music.buffer.borrowed());played22050=info.samples_played;publications+=info.stream_publications;
    check(info.streaming&&info.samples_played>22050&&info.stream_buffers_completed>20&&info.stream_publications>=5,"22050Hz PCM was not consumed/refilled");
    check(f.stream.field_df2c>f.stream.field_df0c&&f.stream.field_df28<f.stream.field_df0c,"source play/refill ring did not wrap");
    // Missing files clear the source active flag but leave an old buffer alive.
    start_audio_music(f.world(),f.track,16,1);
    check(f.stream.playback_active==0&&f.music.buffer==first_id&&f.stream.allocated==1,"missing resource branch changed stream ownership");
    unsigned status{};check(f.device.status(first_id.borrowed(),status)==0&&(status&1u),"missing resource stopped old stream contrary to source");
    f.stream.playback_active=1;stop_audio_music(f.world());
    check(!f.stream.playback_active&&!f.stream.timer&&f.stream.field_df30==0&&f.stream.field_df2c==0,"source stop failed to reset counters");
    check(f.platform.seek_wave(f.stream.file,0,1)==static_cast<int>(f.stream.data_offset+4u),"source stop seek omitted +4");
    check(f.device.inspect(first_id.borrowed()).samples_played==0&&!f.device.inspect(first_id.borrowed()).started,"native rewind did not reset consumed cursor");
    check(start_audio_stream(f.world())==0,"source restart failed");pump(f.platform,50);
    check(f.device.inspect(first_id.borrowed()).samples_played>0,"native restart did not consume PCM");f.close();
    check(f.device.live_buffers()==0&&f.platform.live_files()==0&&f.platform.live_timers()==0,"source close leaked native owners");
    // Every imported file goes through the actual source parser and OS opener.
    for(unsigned selected=2;selected<=23;++selected){
        if(selected>=16&&selected<=19)continue;
        start_audio_music(f.world(),f.track,selected,1);++opened;
        check(f.stream.allocated&&f.stream.result==0&&f.stream.format.channels==2&&f.stream.format.bits_per_sample==16&&
              f.stream.format.block_align==4&&f.stream.format.extra_bytes==0,"imported PCM source format failed");
        check(f.stream.format.samples_per_second==(selected==21?32000u:22050u),"actual original sample rate changed");
        if(selected==21){pump(f.platform,1850);info=f.device.inspect(f.music.buffer.borrowed());played32000=info.samples_played;publications+=info.stream_publications;
            check(info.samples_played>32000&&info.stream_buffers_completed>20&&info.stream_publications>=5,"32000Hz PCM not consumed/refilled");}
    }
    f.close();check(f.device.live_buffers()==0&&f.platform.live_files()==0&&f.platform.live_timers()==0,"repeated open/close leaked resources");
    // The original low-latency/default/large presets must all fit the host
    // transport, including the 32000Hz large ring above the old SFX4MiB cap.
    for(const auto preset:{8u,0x10u,0x20u,0x40u}){
        check(open_audio_stream(f.world(),"sound/Music/Track21.wav",preset,0,0,0,0)==0&&f.stream.allocated,
            "source buffer preset rejected by modern transport");
        f.close();
    }
    check(open_audio_stream(f.world(),"sound/Music/Track21.wav",0x26,0,0,0,0)==0&&!f.stream.allocated&&f.platform.live_files()==0,
        "asynchronous opener ran before owner message dispatch");
    pump(f.platform,40);
    check(f.stream.allocated&&f.device.inspect(f.music.buffer.borrowed()).streaming&&f.device.inspect(f.music.buffer.borrowed()).samples_played>0,
        "asynchronous opener did not create real playback");
    f.close();
    check(f.device.live_buffers()==0&&f.platform.live_files()==0&&f.platform.live_timers()==0,"asynchronous stream close leaked owners");
    {
        Fixture invalid(argv[2]);invalid.track.selected=2;
        start_audio_music(invalid.world(),invalid.track,0,1);
        check(invalid.stream.playback_active==1&&invalid.stream.result==0x88780064&&!invalid.stream.allocated&&
              invalid.device.live_buffers()==0&&invalid.platform.live_files()==0&&invalid.platform.live_timers()==0,
              "source parser error/active flag must not manufacture a native voice");
        start_audio_music(invalid.world(),invalid.track,3,1);
        check(!invalid.stream.playback_active&&invalid.device.live_buffers()==0,"zero length file started music");
    }
    // Cancelled or queued OS messages must not call a captured dead owner.
    unsigned callbacks=0;const auto timer=f.platform.start_timer(1,1,[&]{++callbacks;},1);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));MSG waiting{};
    check(timer!=0&&PeekMessageW(&waiting,nullptr,WM_TIMER,WM_TIMER,PM_NOREMOVE),"timer message was not actually pending");
    check(f.platform.kill_timer(timer)==0,"timer cancellation failed");pump(f.platform,30);
    check(callbacks==0,"cancelled callback ran");
    f.platform.start_open_worker([&]{++callbacks;});f.platform.cancel_pending_callbacks();pump(f.platform,10);
    check(callbacks==0,"cancelled opener ran");
    f.stream.field_df54=0x11223344;f.stream.filename_tail.fill(0xab);reset_audio_stream(f.stream,f.music);
    check(f.stream.field_df54==0x11223300&&f.stream.filename_tail.front()==0xab,"source filename low-byte reset lost retained data");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"actual_files_opened\":"<<opened
        <<",\"samples_consumed_22050\":"<<played22050<<",\"samples_consumed_32000\":"<<played32000
        <<",\"ring_publications\":"<<publications<<",\"master_gain\":0,\"audible_quality_verified\":false,\"whole_source_equivalence\":false}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
