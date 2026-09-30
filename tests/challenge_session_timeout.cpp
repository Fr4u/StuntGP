#include "sgn/playable_driver.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include <cmath>
#include <iostream>
#include <windows.h>

using namespace sgn;using namespace sgn::retail;
namespace {
void require(bool ok,const char* message){if(!ok)throw FormatError(message);}
class Images final:public TextureReplacementDevice {
public:
    Image image;unsigned uploads{};
    explicit Images(const AssetStore& assets):image(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(unsigned slot)const override{require(slot==15,"challenge overlay destination");return static_cast<std::uint16_t>(image.width);}
    void upload_to_destination(unsigned slot,const Image& source)override{require(slot==15,"challenge overlay upload");image=source;++uploads;}
};
void pump(){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
}
int main(int argc,char** argv)try{
    if(argc!=2)return 2;RaceSessionLaunch launch;
    launch.mode=SessionMode::challenge;launch.level_name="CHALL1";launch.master_gain=0;launch.music_percent=100;
    RaceSession session(argv[1],launch);auto& s=session.state();
    require(s.level.game_type==3&&s.clock.lap_count==100&&s.clock.remaining_ticks==5400&&s.level.texture_suffix==2,"source challenge overrides");
    require(s.level.camera_mode==4,"445120 unexpectedly overwrote authored camera mode");
    require(s.clock.game_mode==1&&s.clock.phase==2&&s.clock.countdown_pending==4,"source challenge cold prestart");
    Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());PlayableDriver driver(session,textures,{});
    unsigned steps=0,started=0,time_up=0,finished=0;bool source_camera=false,music=false;
    while(steps<9000&&s.normal_finish.menu_requested!=1){
        pump();auto camera=camera_frame_world(session);camera_frame_begin_outer(camera);
        //Neutral live input. No race clock, score, finish or vehicle pose writes.
        driver.tick({});maintain_session_audio(session);++steps;
        source_camera|=s.controls.controllers[0].camera.mode==7;
        if(s.clock.game_mode==3&&s.clock.phase==2&&started==0)started=steps;
        if(s.score_block!=0&&time_up==0)time_up=steps;
        if(s.vehicles.physics[0].energy_exempt==1&&finished==0)finished=steps;
        music|=s.audio_stream.file.value!=0&&s.audio_stream.playback_active!=0&&s.audio_maintenance.music.buffer.value!=0;
        for(unsigned i=0;i<6;++i)if(s.vehicles.race[i].active==1){const auto p=s.vehicles.physics[i].position;
            require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"challenge nonfinite position");}
    }
    require(started!=0&&time_up-started==5400,"full source challenge time did not elapse");
    require(finished-time_up==179,"source challenge ending countdown differs");
    require(source_camera&&music&&s.normal_finish.menu_requested==1,"challenge adapters/scene request incomplete");
    require(s.clock.phase==3&&s.clock.remaining_ticks==0&&s.score_block==0,"challenge did not finish by timeout");
    const auto& saved=s.selection[0].previous_race;const auto& race=s.vehicles.race[0];
    require(saved.stunt_score==race.stunt_score&&saved.total_ticks==race.total_ticks&&saved.race_timer==race.race_timer&&saved.lap_index==race.lap_index,"challenge result selection save differs");
    require(s.options.selection_phase==2&&s.scene_mode==1,"challenge source transition request differs");
    const auto& presentation=s.vehicles.startup[0].fields_768c;
    require(presentation[6]==0&&presentation[11]==0,"challenge incorrectly entered normal podium/results");
    std::cout<<"{\"passed\":true,\"level\":\"CHALL1\",\"world_ticks\":"<<steps<<",\"race_started\":"<<started
        <<",\"time_up\":"<<time_up<<",\"finished\":"<<finished<<",\"timed_ticks\":5400,\"source_camera7\":"<<source_camera
        <<",\"saved_stunt_score\":"<<saved.stunt_score<<",\"image_uploads\":"<<images.uploads<<",\"music_file_and_buffer\":"<<music
        <<",\"menu_requested\":"<<s.normal_finish.menu_requested<<",\"direct_finish_or_pose_writes\":false,\"menu_resource_chain\":false,\"gpu_test\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
