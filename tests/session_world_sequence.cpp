#include "sgn/race_session_world_tick.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/retail_hud_lifetime.hpp"
#include "sgn/retail_tick_clock.hpp"
#include <cmath>
#include <iostream>
#include <string>

// Empty-input world integration probe. The actual playable driver has its own
// input/reset/gate/music/platform tests; this does not claim a complete frame.
namespace {
using namespace sgn;using namespace sgn::retail;
class Images final:public TextureReplacementDevice {
public:
    Image actual;unsigned uploads{};
    explicit Images(const AssetStore& assets):actual(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(unsigned slot)const override{if(slot!=15||actual.width!=256)throw FormatError("sequence overlay destination");return 256;}
    void upload_to_destination(unsigned slot,const Image& value)override{if(slot!=15)throw FormatError("sequence overlay upload");actual=value;++uploads;}
};
struct Trace {
    unsigned tick{},stage{},vehicle_stage{},slot{},type8_before{},type8_updates{};
    const RaceSessionState* session{};
};
}
int main(int argc,char** argv)try{
    if(argc<2||argc>4)throw FormatError("expected retail root, optional track number and launch-effect probe");
    RaceSessionLaunch launch;launch.master_gain=0;
    const unsigned track=argc>=3?std::stoul(argv[2]):1;
    const bool launch_probe=argc==4&&std::string_view(argv[3])=="launch-effect";
    const bool normal_probe=argc==4&&std::string_view(argv[3])=="normal-race";
    if(argc==4&&!launch_probe&&!normal_probe)throw FormatError("unknown sequence probe");
    if(normal_probe)launch.mode=SessionMode::normal_race;
    if(track<1||track>30)throw FormatError("track number outside authored range");
    launch.level_name="TRACK"+std::to_string(track);
    RaceSession session(argv[1],launch);auto& s=session.state();
    Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());Trace trace;trace.session=&s;
    VehicleWorldTickAudit vehicle{};vehicle.context=&trace;vehicle.before_stage=[](void* p,unsigned entry,unsigned slot)noexcept{auto& t=*static_cast<Trace*>(p);t.vehicle_stage=entry;t.slot=slot;};
    SessionWorldTickAudit audit{};audit.context=&trace;audit.vehicle=&vehicle;
    audit.before_stage=[](void* p,unsigned entry)noexcept{
        auto& t=*static_cast<Trace*>(p);t.stage=entry;
        if(entry==0x415df0){t.type8_before=0;
            for(const auto& particle:t.session->impact_particles.particles)
                if(particle.type==8&&particle.lifetime>0)++t.type8_before;}
    };
    audit.after_stage=[](void* p,unsigned entry)noexcept{
        auto& t=*static_cast<Trace*>(p);if(entry==0x415df0)t.type8_updates+=t.type8_before;
    };
    const auto start=s.vehicles.physics[0].position;
    std::array<VehicleVector,6> starting_positions;
    for(unsigned i=0;i<6;++i){starting_positions[i]=s.vehicles.physics[i].position;
        if(normal_probe&&s.vehicles.race[i].active!=1)throw FormatError("normal preset did not create six actual cars");}
    unsigned manual_tick{};
    try{for(unsigned n=0;n<1500;++n){trace.tick=n;auto camera=camera_frame_world(session);camera_frame_begin_outer(camera);
        // Targeted source-producer integration probe, not a natural-input run:
        // invoke actual4163E0 once;416800/416590 create particles during the
        // real vehicle stage. Never author particle types or suppress lifetime.
        if(launch_probe&&n==800)set_launch_effect(s.vehicles.physics[0],1);
        tick_session_world(session,textures,{},&audit);
        update_controller_overlays(s.controls,s.overlays);
        update_race_hud({s.controls,s.hud_countdown,s.hud_layout,s.selected_hud,s.hud_margins},s.vehicles.physics,s.vehicles.race);
        advance_retail_tick(s.simulation_tick,s.tick_clock);
        const auto& player_race=s.vehicles.race[0];
        if(manual_tick==0&&s.clock.game_mode==(normal_probe?2u:4u)&&s.clock.phase==2&&player_race.control_enabled==1&&player_race.automatic_drive==0)
            manual_tick=s.simulation_tick;
        for(unsigned i=0;i<6;++i){const auto& p=s.vehicles.physics[i].position;if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw FormatError("sequence nonfinite position");}
        if(n<5||n%60==59)std::cerr<<"tick="<<s.simulation_tick<<" mode="<<s.clock.game_mode<<" phase="<<s.clock.phase
            <<" pending="<<s.clock.countdown_pending<<" countdown="<<s.vehicles.race[0].start_countdown
            <<" position="<<s.vehicles.physics[0].position.x<<','<<s.vehicles.physics[0].position.y<<','<<s.vehicles.physics[0].position.z
            <<" speed="<<s.vehicles.physics[0].speed<<" texture="<<textures.selected_overlay_texture()
            <<" controls="<<player_race.control_enabled<<" automatic="<<player_race.automatic_drive
            <<" race_timer="<<player_race.race_timer<<'\n';
    }}catch(const std::exception& e){std::cerr<<"FAILED tick="<<trace.tick<<" stage="<<std::hex<<trace.stage<<" vehicle_stage="<<trace.vehicle_stage<<std::dec<<" slot="<<trace.slot<<" error="<<e.what()<<'\n';return 1;}
    const auto end=s.vehicles.physics[0].position;
    if(manual_tick==0)throw FormatError("sequence did not reach manual control");
    if(launch_probe&&trace.type8_updates==0)throw FormatError("launch probe never updated a produced type8 particle");
    unsigned moved_rivals{};
    if(normal_probe)for(unsigned i=1;i<6;++i){const auto p=s.vehicles.physics[i].position,b=starting_positions[i];
        const double dx=double(p.x)-b.x,dy=double(p.y)-b.y,dz=double(p.z)-b.z;
        if(dx*dx+dy*dy+dz*dz>10000)++moved_rivals;
    }
    if(normal_probe&&moved_rivals!=5)throw FormatError("normal preset did not advance all five actual AI cars");
    std::cout<<"{\"passed\":true,\"track\":"<<track<<",\"ticks\":"<<s.simulation_tick<<",\"mode\":"<<s.clock.game_mode<<",\"phase\":"<<s.clock.phase
        <<",\"countdown\":"<<s.vehicles.race[0].start_countdown<<",\"uploads\":"<<images.uploads
        <<",\"manual_control_tick\":"<<manual_tick<<",\"race_timer\":"<<s.vehicles.race[0].race_timer
        <<",\"direct_source_effect_trigger\":"<<std::boolalpha<<launch_probe<<",\"type8_particle_updates\":"<<trace.type8_updates
        <<",\"normal_race\":"<<normal_probe<<",\"moved_rivals\":"<<moved_rivals
        <<",\"player_displacement\":"<<std::sqrt(double(end.x-start.x)*(end.x-start.x)+double(end.y-start.y)*(end.y-start.y)+double(end.z-start.z)*(end.z-start.z))
        <<",\"full_frame_test\":false,\"source_parity\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
