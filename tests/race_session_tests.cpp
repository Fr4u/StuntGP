#include "sgn/race_session.hpp"
#include <cmath>
#include <iostream>
int main(int argc,char** argv){try{
    using namespace sgn::retail;
    if(argc!=2)throw std::runtime_error("expected retail root");
    RaceSessionLaunch launch;launch.master_gain=0;
    RaceSession session(argv[1],launch);auto& s=session.state();
    auto require=[](bool v,const char* error){if(!v)throw std::runtime_error(error);};
    require(s.clock.game_mode==1&&s.clock.phase==2&&s.clock.race_mode==3&&s.clock.countdown_pending==4,"source startup phase mismatch");
    require(s.level.game_type==4&&s.current_slot==0&&s.controls.current==3,"startup identities mismatch");
    require(s.clock.course_length>0&&std::isfinite(s.clock.course_length),"course length invalid");
    require(&session.scene_resources().strip_geometry()==&s.strip,"strip owner was copied");
    const auto driver=session.audio_view();require(&driver.controls==&s.controls&&&driver.level==&s.audio_level&&&driver.split_setting==&s.split_setting,"audio aliases mismatch");
    require(s.audio_driver.initialized==1&&session.audio_device().live_buffers()>0,"actual audio device did not load original PCM");
    require(s.gameplay.pending==0&&s.score_rules.pending==0&&s.gameplay.values[1]>0,"authored rules were not loaded");
    require(s.audio_level.scale==100&&s.audio_level.output_b==0&&s.audio_level.output_b_copy==255,"platform audio settings lost source460C20/40D500 results");
    require(s.viewport.full_width==launch.width&&s.viewport.full_height==launch.height&&s.viewport.requested_selector==1,"source viewport startup dimensions");
    require(s.color_tables.raster_gamma.front()==2048&&s.color_tables.raster_gamma.back()==65280,"color producer not called");
    require(s.contact_cast.mode==1,"pinned contact query mode missing");
    require(s.gravity.factor==4&&s.gravity.inverse_factor==0.25f,"source445FA0 gravity override missing");
    require(!session.text(98).empty()&&!session.raw_texture_labels().empty(),"authored resources absent");
    for(unsigned i=0;i<6;++i){const auto& p=s.vehicles.physics[i].position;
        require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"vehicle startup position not finite");}
    std::cout<<"level="<<session.level_name()<<" mode="<<s.clock.game_mode<<" phase="<<s.clock.phase
        <<" pending="<<s.clock.countdown_pending<<" length="<<s.clock.course_length
        <<" models="<<session.models().size()<<" audio_buffers="<<session.audio_device().live_buffers()<<" errors=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
