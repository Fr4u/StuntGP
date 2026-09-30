#include "sgn/playable_driver.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include <iostream>
#include <windows.h>
using namespace sgn;using namespace sgn::retail;
namespace {
class Images final:public TextureReplacementDevice {
public:
    Image image;unsigned uploads{};
    explicit Images(const AssetStore& assets):image(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(unsigned slot)const override{if(slot!=15||image.width>65535)throw FormatError("finish overlay slot/width");return static_cast<std::uint16_t>(image.width);}
    void upload_to_destination(unsigned slot,const Image& source)override{if(slot!=15)throw FormatError("finish overlay upload slot");image=source;++uploads;}
};
void pump(){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
}
int main(int argc,char** argv)try{
    if(argc!=2)return 2;RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;launch.music_percent=100;
    RaceSession session(argv[1],launch);auto& s=session.state();Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
    PlayableDriver driver(session,textures,{});
    const auto tick=[&]{pump();auto camera=camera_frame_world(session);camera_frame_begin_outer(camera);driver.tick({});maintain_session_audio(session);};
    for(unsigned n=0;n<320;++n)tick();
    if(s.clock.phase!=2||s.clock.game_mode!=2)throw FormatError("normal finish probe did not start");
    //Explicit fixture: invoke the real41DA70 producer, not a naturally completed lap.
    //No direct writes to energy_exempt, place, timer, or phase.
    VehicleRaceFinishWorld finish{s.vehicles.physics,s.vehicles.race,s.ranking,s.finish,s.clock,s.records,s.hud_block};
    //The source's automatic completion uses the live ranking starting at
    //finish.count; invoke producers in that order, preserving its precondition.
    const auto order=s.ranking.slots;
    for(const auto slot:order){if(!slot||*slot>=6)throw FormatError("finish fixture ranking identity");
        if(s.vehicles.physics[*slot].energy_exempt==0)finish_vehicle_race(finish,*slot);}
    if(s.finish.count!=6)throw FormatError("finish producer did not publish six results");
    bool camera=false,podium=false,results=false,music=false;unsigned post=0,results_at=0;
    while(post<2000&&!s.normal_finish.menu_requested){
        tick();++post;const auto& f=s.vehicles.startup[0].fields_768c;
        camera|=s.controls.controllers[0].camera.mode==10;podium|=f[11]!=0;results|=f[6]!=0;
        music|=s.audio_stream.allocated!=0&&s.audio_stream.playback_active!=0&&s.audio_stream.file.value!=0&&s.audio_maintenance.music.buffer.value!=0;
        if(f[6]==1&&s.normal_finish.visible_rows==6&&results_at==0)results_at=post;
        if(results_at!=0&&post==results_at+60){s.normal_finish.confirm_consumed=0;s.normal_finish.confirm_pressed=1;}
    }
    if(!camera||!results||!music||!s.normal_finish.menu_requested)throw FormatError("normal finish session did not complete its actual adapters");
    std::cout<<"{\"passed\":true,\"prestart_ticks\":320,\"finish_world_ticks\":"<<post
        <<",\"camera10\":"<<camera<<",\"podium\":"<<podium<<",\"results\":"<<results
        <<",\"music_file_and_buffer\":"<<music<<",\"visible_rows\":"<<s.normal_finish.visible_rows
        <<",\"menu_requested\":"<<s.normal_finish.menu_requested<<",\"uploads\":"<<images.uploads
        <<",\"direct_41da70_fixture\":true,\"natural_lap_proof\":false,\"audible_playback_proof\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
