#include "sgn/platform_audio.hpp"
#include "sgn/retail_audio_catalog.hpp"
#include "sgn/retail_audio_maintenance.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
#include <windows.h>

namespace {
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void ok(sgn::retail::AudioResult value,const char* message){if(value!=0)throw sgn::AudioError(value,message);}
}
int main(int argc,char** argv){try{
    using namespace sgn;using namespace sgn::retail;
    if(argc!=2)throw std::runtime_error("expected retail data root");AssetStore assets(argv[1]);
    // Exercise the actual output device and mixer with an explicitly muted
    // mastering voice. SamplesPlayed proves consumption, not audible quality.
    WindowsAudioDevice device(0);AudioDriverState driver;device.load_catalog(assets,driver);
    require(device.live_buffers()==71&&driver.samples[16].frequency==11050&&driver.samples[0].buffer.value==0,"hardware catalog identities/rates");
    const auto mono=driver.samples[17].buffer;const auto spatial=driver.samples[1];AudioBufferId duplicate;
    ok(device.duplicate(mono,duplicate),"duplicate2D");require(duplicate!=mono&&device.live_buffers()==72,"independent audio duplicate identity");
    ok(device.volume(duplicate,-2500),"set duplicate volume");ok(device.frequency(duplicate,22050),"set frequency");ok(device.pan(duplicate,10000),"pan right");
    const auto right=device.inspect(duplicate).output_matrix;ok(device.pan(duplicate,-10000),"pan left");
    if(device.info().channels>=2)require(right!=device.inspect(duplicate).output_matrix,"stereo pan did not change real matrix");
    require(device.volume(duplicate,1)!=0&&device.pan(duplicate,10001)!=0&&device.frequency(duplicate,0xffffffff)!=0,"invalid source audio range accepted");
    std::uint32_t flags=0x1234;require(device.status({0xffffffffffffffffull},flags)!=0&&flags==0x1234,"invalid buffer status fabricated a result");
    AudioSpatialId absent{123};require(device.query_spatial(mono,absent)!=0&&absent.value==123,"2D source exposed spatial interface");
    ok(device.play(duplicate,0,0,1),"start looping audio");ok(device.status(duplicate,flags),"read playing status");require((flags&5)==5,"looping status missing");
    std::uint64_t processed=0;for(unsigned retry=0;retry<100;++retry){processed=device.inspect(duplicate).samples_played;if(processed)break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    require(processed>0,"real audio engine did not consume samples");ok(device.stop(duplicate),"stop looping audio");ok(device.status(duplicate,flags),"read stopped status");require(flags==0,"stopped audio reported playing");
    require(device.inspect(duplicate).queued==1,"Stop discarded audio cursor/queue");ok(device.play(duplicate,0,0,1),"resume retained audio");ok(device.stop(duplicate),"stop resumed audio");
    require(device.play(duplicate,0,0,0)!=0,"unimplemented queued loop-mode switch falsely succeeded");
    const auto listener=device.listener();ok(device.listener_position(listener,{0,0,0},0),"listener position");ok(device.listener_velocity(listener,{0,0,0},0),"listener velocity");ok(device.listener_orientation(listener,{0,0,1},{0,1,0},0),"listener basis");
    auto parameters=audio_default_spatial_parameters();parameters.position={2000,0,0};ok(device.spatial_parameters(spatial.spatial,parameters,0),"spatial template");
    const auto positioned=device.inspect(spatial.buffer).output_matrix;ok(device.spatial_position(spatial.spatial,{-2000,0,0},0),"spatial source move");
    if(device.info().channels>=2)require(positioned!=device.inspect(spatial.buffer).output_matrix,"spatial move did not change speaker matrix");
    ok(device.spatial_velocity(spatial.spatial,{0,0,0},0),"spatial velocity");ok(device.spatial_mode(spatial.spatial,2,0),"nonpositioned3D mode");ok(device.spatial_mode(spatial.spatial,0,0),"positioned3D mode");
    require(device.spatial_position(spatial.spatial,{std::numeric_limits<float>::quiet_NaN(),0,0},0)!=0&&device.listener_orientation(listener,{0,0,0},{0,1,0},0)!=0,"invalid spatial data accepted");
    ok(device.spatial_position(spatial.spatial,{100000,0,0},0),"distant source");const auto distant_matrix=device.inspect(spatial.buffer).output_matrix;require(std::all_of(distant_matrix.begin(),distant_matrix.end(),[](float v){return v==0;}),"source maximum distance mute omitted");
    AudioBufferId spatial_duplicate;ok(device.duplicate(spatial.buffer,spatial_duplicate),"duplicate3D");AudioSpatialId spatial_interface;ok(device.query_spatial(spatial_duplicate,spatial_interface),"query duplicate3D");
    require(device.release(spatial_duplicate)==1&&device.live_buffers()==73,"buffer released while spatial reference remains");
    require(device.release_spatial(spatial_interface)==0&&device.live_buffers()==72,"final spatial release leaked voice");
    require(device.release(duplicate)==0&&device.live_buffers()==71,"duplicate voice release leaked");
    LevelAudioState level{};level.output_b=255;contact::player::Controls controls{};controls.controllers[0].active=1;std::uint32_t split=0;
    AudioDriver recovered{driver,device,level,controls,split};audio_set_range(driver,7500);
    std::uint32_t first_handle=0,second_handle=0,third_handle=0,fourth_handle=0;
    audio_start(recovered,17,1,0,255,0,0,&first_handle);audio_start(recovered,17,1,0,255,0,0,&second_handle);
    audio_start_positioned(recovered,1,1,0,255,{10,0,10},nullptr,0,&third_handle);
    audio_start_positioned(recovered,1,1,0,255,{20,0,10},nullptr,0,&fourth_handle);
    if(first_handle!=0||second_handle!=72||third_handle!=1||fourth_handle!=73||driver.duplicate_count!=2||device.live_buffers()!=73)
        std::cerr<<"driver handles="<<first_handle<<','<<second_handle<<','<<third_handle<<','<<fourth_handle<<" duplicates="<<driver.duplicate_count<<" voices="<<device.live_buffers()<<'\n';
    // Unpositioned source start retains the caller's handle when no duplicate
    // was needed; positioned start publishes the base slot before duplication.
    require(first_handle==0&&second_handle==72&&third_handle==1&&fourth_handle==73&&driver.duplicate_count==2&&device.live_buffers()==73,"recovered driver did not allocate real2D/3D duplicate pool");
    audio_recount_playing(recovered);require(driver.playing_count==4,"source recount did not read real voice status");
    audio_stop_all(recovered);audio_recycle_duplicates(recovered);audio_recount_playing(recovered);
    require(driver.duplicate_count==0&&driver.playing_count==0&&device.live_buffers()==71,"recovered stop/recycle leaked real buffer or spatial interface");
    for(auto& sample:driver.samples){if(sample.spatial.value)device.release_spatial(sample.spatial);if(sample.buffer.value)device.release(sample.buffer);sample={};}
    require(device.live_buffers()==0&&device.health()==0,"audio shutdown leaked resources or lost device");
    AudioDriverState startup_driver;startup_driver.spatial_mode=1;startup_driver.spatial_template=audio_default_spatial_parameters();
    AudioInitializationState initialization;initialization.requested_rate=44100;AudioMaintenanceState maintenance;AudioStreamState stream;LevelOverrideOptions options;
    level.scale=100;level.input_a=0;level.input_b=100;level.output_a=0;split=0;
    AudioDriver startup_audio{startup_driver,device,level,controls,split};WindowsAudioStreamPlatform streams(device);
    AudioStartupWorld startup{startup_audio,maintenance,stream,initialization,level,options,assets,device,streams,reinterpret_cast<std::uint64_t>(GetDesktopWindow())};
    require(initialize_audio(startup)&&device.live_buffers()==71&&startup_driver.initialized==1&&startup_driver.spatial_mode==1,"source initialization did not create actual71voice catalog");
    require(initialization.enumeration.records[0]!=0&&maintenance.listener==device.listener(),"real audio endpoint enumeration/listener missing");
    for(const auto& definition:audio_sample_definitions){const auto& sample=startup_driver.samples[definition.slot];require(sample.type==definition.type&&sample.frequency!=0&&sample.buffer.value!=0,"startup sample mapping differs");}
    const auto startup_voice=startup_driver.samples[17].buffer;
    ok(device.frequency(startup_voice,22050),"configure source pitch before rewind");ok(device.pan(startup_voice,-2500),"configure source pan before rewind");
    const auto before_rewind=device.inspect(startup_voice);ok(device.play(startup_voice,0,0,1),"play source-initialized voice");
    std::uint64_t startup_processed=0;for(unsigned retry=0;retry<100;++retry){startup_processed=device.inspect(startup_voice).samples_played;if(startup_processed)break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    require(startup_processed>0,"source-loaded audio did not reach real mixer");ok(device.stop(startup_voice),"stop source-initialized voice");ok(device.set_play_cursor(startup_voice,0),"reset source audio cursor");require(device.inspect(startup_voice).queued==0,"source cursor reset retained old queue");
    const auto after_rewind=device.inspect(startup_voice);require(after_rewind.samples_played==0&&!after_rewind.started&&after_rewind.references==before_rewind.references&&after_rewind.frequency_ratio==before_rewind.frequency_ratio&&after_rewind.output_matrix==before_rewind.output_matrix&&device.live_buffers()==71,"rewind changed source identity, pitch, pan or ownership");
    ok(device.play(startup_voice,0,0,0),"one-shot after loop rewind");require(device.inspect(startup_voice).queued==1,"rewound source failed immediate replay");ok(device.stop(startup_voice),"stop rewound one-shot");
    const auto space=startup_driver.samples[1];ok(device.spatial_position(space.spatial,{2000,0,0},0),"place source emitter");
    const auto spatial_before_rewind=device.inspect(space.buffer);ok(device.set_play_cursor(space.buffer,0),"rewind source with spatial reference");const auto spatial_after_rewind=device.inspect(space.buffer);
    require(spatial_after_rewind.references==2&&spatial_after_rewind.output_matrix==spatial_before_rewind.output_matrix,"rewind lost spatial interface or emitter state");
    ok(device.spatial_velocity(space.spatial,{0,0,0},0),"spatial interface after rewind");
    const auto summed=[](const std::vector<float>& v){double sum=0;for(float x:v)sum+=x;return sum;};
    ok(device.listener_rolloff(device.listener(),0,0),"zero source rolloff");const auto flat=summed(device.inspect(space.buffer).output_matrix);
    ok(device.listener_rolloff(device.listener(),6,0),"source rolloff6");const auto faded=summed(device.inspect(space.buffer).output_matrix);require(faded<flat&&faded>=0,"source rolloff ignored by actual mixer");
    split=1;reconfigure_audio_for_split(startup);require(device.live_buffers()==71&&startup_driver.spatial_mode==0,"split reconfiguration leaked original generation");
    for(const auto& sample:startup_driver.samples)if(sample.buffer.value)require(sample.type==1&&sample.spatial.value==0,"split catalog retained3D interfaces");
    flags=77;require(device.status(startup_voice,flags)!=0&&flags==77,"released generation buffer still accepted");
    split=0;reconfigure_audio_for_split(startup);require(device.live_buffers()==71&&startup_driver.spatial_mode==1,"restored3D catalog missing");
    shutdown_audio(startup);require(device.live_buffers()==0&&startup_driver.initialized==1&&initialization.device.value==0,"source shutdown ownership/status changed");
    std::cout<<"{\"passed\":true,\"catalog\":71,\"device_channels\":"<<device.info().channels<<",\"device_rate\":"<<device.info().sample_rate<<",\"samples_consumed\":"<<processed<<",\"startup_samples_consumed\":"<<startup_processed<<",\"source_initializations\":3,\"source_reconfigurations\":2,\"recovered_driver_duplicates\":2,\"live_buffers_after_release\":0,\"master_gain\":0,\"audible_quality_verified\":false}\n";return 0;
}catch(const sgn::AudioError& e){std::cerr<<e.what()<<" HRESULT="<<std::hex<<static_cast<unsigned>(e.result)<<'\n';return 2;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
