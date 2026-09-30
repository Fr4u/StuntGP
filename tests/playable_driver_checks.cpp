#include "sgn/playable_driver.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool ok,const char* reason){if(!ok)throw FormatError(reason);}
class Images final:public TextureReplacementDevice {
public:
    Image actual;unsigned uploads{};
    explicit Images(const AssetStore& assets):actual(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(unsigned slot)const override{require(slot==15&&actual.width==256,"overlay destination");return 256;}
    void upload_to_destination(unsigned slot,const Image& image)override{require(slot==15,"overlay slot");actual=image;++uploads;}
};
struct Trace {unsigned tick{},stage{},vehicle_stage{},slot{},car_call{};};
struct Result {
    unsigned ticks{},manual_tick{},manual_steps{},uploads{},reset_requests{},reset_consumed{};
    double distance{},max_speed{},max_throttle{},max_brake{},left_steering{},right_steering{};
    VehicleVector at_throttle_end{},final_position{};
};
double distance(VehicleVector a,VehicleVector b){
    return std::sqrt((double(a.x)-b.x)*(double(a.x)-b.x)+(double(a.y)-b.y)*(double(a.y)-b.y)+(double(a.z)-b.z)*(double(a.z)-b.z));
}
unsigned primary(const RaceSessionState& s){
    for(const auto& c:s.controls.controllers)if(c.active==1&&c.vehicle<6)return c.vehicle;
    throw FormatError("test has no active controller");
}
Result run(const std::filesystem::path& root,const std::filesystem::path& folder,bool driven){
    RaceSessionLaunch launch;launch.master_gain=0;launch.music_percent=0;
    RaceSession session(root,launch);auto& s=session.state();Images images(session.assets());
    RaceOverlayTextures textures(session.assets(),images,session.shared_platform());PlayableDriver driver(session,textures,{});
    require(s.audio_driver.initialized==1,"real SFX device did not initialize");
    require(s.audio_level.output_a==0,"music disabled source guard");
    std::ofstream csv(folder/(driven?"driven.csv":"neutral.csv"));require(bool(csv),"trace creation");
    csv<<std::setprecision(9)<<"tick,manual,slot,mode,phase,automatic,enabled,countdown,hold,input_gas,input_brake,input_left,input_right,previous_gas,previous_brake,previous_left,previous_right,previous_shift,previous_stunt,x,y,z,speed,throttle,steering,brake_ramp,gear,rng,reset_pending\n";
    Trace trace;VehicleTickAudit calls;calls.call_context=&trace;
    calls.before_call=[](void* c,unsigned address)noexcept{static_cast<Trace*>(c)->car_call=address;};
    VehicleWorldTickAudit world{&calls,&trace,[](void* c,unsigned entry,unsigned slot)noexcept{
        auto& t=*static_cast<Trace*>(c);t.vehicle_stage=entry;t.slot=slot;}};
    SessionWorldTickAudit audit;audit.vehicle=&world;audit.context=&trace;
    audit.before_stage=[](void* c,unsigned entry)noexcept{static_cast<Trace*>(c)->stage=entry;};
    Result result;bool manual=false;VehicleVector previous=s.vehicles.physics[0].position;
    try{
        for(unsigned step=0;step<3000&&result.manual_steps<660;++step){
            trace.tick=step;const auto slot=primary(s);
            const auto& race=s.vehicles.race.at(slot);
            if(!manual&&s.clock.phase==2&&s.clock.game_mode==4&&race.automatic_drive==0&&race.control_enabled!=0){
                manual=true;result.manual_tick=s.simulation_tick;
            }
            PlayableInput input;
            const auto n=result.manual_steps;
            if(driven&&manual){
                input.throttle=n<240||(n>=330&&n<390)||n>=391;
                input.left=n>=180&&n<210;input.right=n>=210&&n<240;
                input.brake=n>=270&&n<330;input.reset=n==390;
            }
            auto camera=camera_frame_world(session);camera_frame_begin_outer(camera);
            driver.tick(input,&audit);
            if(input.reset){
                ++result.reset_requests;const auto& recovered=s.vehicles.race[slot];
                if(recovered.respawn_requested==0&&recovered.input_hold==30&&
                    distance(s.vehicles.physics[slot].position,recovered.respawn_position)==0)++result.reset_consumed;
            }
            require(s.simulation_tick==step+1,"driver must advance exactly one retail tick");
            for(unsigned j=0;j<6;++j){const auto& physical=s.vehicles.physics[j];
                require(std::isfinite(physical.position.x)&&std::isfinite(physical.position.y)&&std::isfinite(physical.position.z)&&std::isfinite(physical.speed),"nonfinite vehicle state");
                for(float value:physical.orientation)require(std::isfinite(value),"nonfinite vehicle orientation");
            }
            const auto& p=s.vehicles.physics[slot];const auto& r=s.vehicles.race[slot];const auto& old=s.vehicles.restart[slot].previous_input;
            if(manual){
                result.distance+=distance(p.position,previous);result.max_speed=std::max(result.max_speed,std::abs(double(p.speed)));
                result.max_throttle=std::max(result.max_throttle,double(p.throttle));result.max_brake=std::max(result.max_brake,double(p.brake_ramp));
                if(n==179)result.at_throttle_end=p.position;
                if(n==209)result.left_steering=p.steering;
                if(n==239)result.right_steering=p.steering;
                ++result.manual_steps;
            }
            previous=p.position;
            csv<<s.simulation_tick<<','<<manual<<','<<slot<<','<<s.clock.game_mode<<','<<s.clock.phase<<','<<r.automatic_drive<<','<<r.control_enabled<<','<<r.start_countdown<<','<<r.input_hold
                <<','<<unsigned(p.input.throttle)<<','<<unsigned(p.input.brake)<<','<<unsigned(p.input.left)<<','<<unsigned(p.input.right)
                <<','<<unsigned(old.throttle)<<','<<unsigned(old.brake)<<','<<unsigned(old.left)<<','<<unsigned(old.right)<<','<<unsigned(p.previous_shift)<<','<<unsigned(p.previous_stunt)
                <<','<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','<<p.speed<<','<<p.throttle<<','<<p.steering<<','<<p.brake_ramp<<','<<p.gear<<','<<s.random.state<<','<<r.respawn_requested<<'\n';
        }
    }catch(const std::exception& error){
        std::cerr<<"scenario="<<(driven?"driven":"neutral")<<" tick="<<trace.tick<<" stage="<<std::hex<<trace.stage<<" vehicle_stage="<<trace.vehicle_stage<<" car_call="<<trace.car_call<<std::dec<<" slot="<<trace.slot<<" error="<<error.what()<<'\n';throw;
    }
    require(result.manual_steps==660,"manual takeover did not occur within real source progression");
    const auto slot=primary(s);
    result.ticks=s.simulation_tick;result.uploads=images.uploads;result.final_position=s.vehicles.physics[slot].position;
    driver.release_input();const auto& released=s.vehicles.physics[slot];
    require(!released.input.throttle&&!released.input.brake&&!released.input.left&&!released.input.right&&!released.input.turbo&&!released.input.stunt&&!released.input.shift,"focus release retained held input");
    require(s.simulation_tick==result.ticks,"input release advanced simulation");
    return result;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw FormatError("expected retail root and output folder");
    const std::filesystem::path folder=argv[2];std::filesystem::create_directories(folder);
    const auto neutral=run(argv[1],folder,false),driven=run(argv[1],folder,true);
    const auto divergence=distance(neutral.at_throttle_end,driven.at_throttle_end);
    require(neutral.manual_tick==driven.manual_tick,"input-free warmup diverged before manual takeover");
    require(divergence>10&&driven.max_throttle>0,"gas did not change actual manual trajectory against neutral reference");
    require(driven.left_steering<0&&driven.right_steering>0,"left/right input did not produce opposing steering");
    require(driven.max_brake>0,"brake input did not activate actual brake ramp");
    require(driven.reset_requests==1&&driven.reset_consumed==1,"real recovery request was not consumed");
    std::cout<<std::setprecision(10)<<"{\"passed\":true,\"ticks_per_scenario\":"<<driven.ticks<<",\"manual_takeover_tick\":"<<driven.manual_tick
        <<",\"manual_steps_per_scenario\":"<<driven.manual_steps<<",\"gas_trajectory_difference\":"<<divergence
        <<",\"driven_max_speed\":"<<driven.max_speed<<",\"neutral_max_speed\":"<<neutral.max_speed
        <<",\"left_steering\":"<<driven.left_steering<<",\"right_steering\":"<<driven.right_steering
        <<",\"max_brake_ramp\":"<<driven.max_brake<<",\"reset_consumed\":"<<driven.reset_consumed
        <<",\"real_overlay_uploads\":"<<driven.uploads<<",\"source_parity\":false,\"gpu_verified\":false}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
