#include "sgn/playable_driver.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

// Integration observation of an authored session, not an original-EXE oracle.
// Only held keyboard actions are injected. No car pose, AI input, activation,
// countdown, contact flag or RNG output is supplied by this executable.
namespace {
using namespace sgn;
using namespace sgn::retail;
constexpr std::array<unsigned,7> world_order{
    0x405210,0x427140,0x45e1d0,0x415df0,0x443a80,0x418a90,0x401770};

class Images final:public TextureReplacementDevice {
    Image image_;
public:
    unsigned uploads{};
    explicit Images(const AssetStore& assets):image_(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(unsigned slot)const override{
        if(slot!=15||image_.width!=256)throw FormatError("contact session overlay destination");
        return 256;
    }
    void upload_to_destination(unsigned slot,const Image& image)override{
        if(slot!=15)throw FormatError("contact session overlay upload");
        image_=image;++uploads;
    }
};

struct Trace {
    const RaceSessionState& state;
    unsigned before{},after{},contact_cursor{},vehicle_cursor{},last_entry{},last_slot{};
    unsigned active_count{},contacts{},advances{},mechanics{},rankings{};
    bool mechanical_phase{},waiting_for_vehicle{};
    bool rival_pending{};
    unsigned rival_slot{};
    std::array<bool,6> active{};
    std::array<std::uint64_t,6> rival_dispatch_returns{};
    const char* error{};
    std::uint64_t total_contacts{},total_mechanics{},world_ticks{};
    explicit Trace(const RaceSessionState& s):state(s){}
    void fail(const char* message)noexcept{if(!error)error=message;}
    void reset()noexcept{
        before=after=contact_cursor=vehicle_cursor=active_count=contacts=advances=mechanics=rankings=0;
        mechanical_phase=waiting_for_vehicle=rival_pending=false;active.fill(false);
    }
    unsigned next(unsigned& cursor)const noexcept{
        while(cursor<6&&!active[cursor])++cursor;
        return cursor++;
    }
    static void before_world(void* pointer,unsigned entry)noexcept{
        auto& t=*static_cast<Trace*>(pointer);t.last_entry=entry;
        if(t.before>=world_order.size()||world_order[t.before]!=entry||t.before!=t.after)
            t.fail("world stage order or nested stage changed");
        ++t.before;
        if(entry==0x418a90)for(unsigned i=0;i<6;++i){
            const auto& race=t.state.vehicles.race[i];t.active[i]=race.active==1;
            if(t.active[i]){
                ++t.active_count;
                if(race.recording&&t.state.replays.status(*race.recording)==1)
                    t.fail("normal race unexpectedly entered replay playback");
            }
        }
    }
    static void after_world(void* pointer,unsigned entry)noexcept{
        auto& t=*static_cast<Trace*>(pointer);
        if(t.after>=world_order.size()||world_order[t.after]!=entry||t.before!=t.after+1)
            t.fail("world stage completion order changed");
        ++t.after;
        if(entry==0x418a90){
            if(t.contacts!=t.active_count||t.advances!=t.active_count||
               t.mechanics!=t.active_count||t.rankings!=1||t.waiting_for_vehicle||t.rival_pending)
                t.fail("world did not execute every eligible contact and vehicle exactly once");
            if(t.state.current_slot!=0)t.fail("world did not restore vehicle selector");
            t.total_contacts+=t.contacts;t.total_mechanics+=t.mechanics;++t.world_ticks;
        }
    }
    static void vehicle(void* pointer,unsigned entry,unsigned slot)noexcept{
        auto& t=*static_cast<Trace*>(pointer);t.last_entry=entry;t.last_slot=slot;
        if(entry==0x409b60){
            if(t.mechanical_phase||slot!=t.next(t.contact_cursor))
                t.fail("contact phase is interleaved, skipped or out of source slot order");
            ++t.contacts;
        }else if(entry==0x41af50){
            t.mechanical_phase=true;
            if(t.contacts!=t.active_count||t.waiting_for_vehicle||slot!=t.next(t.vehicle_cursor))
                t.fail("vehicle phase started before all contacts or reordered slots");
            t.waiting_for_vehicle=true;++t.advances;
        }else if(entry==0x418830){
            if(!t.waiting_for_vehicle||slot+1!=t.vehicle_cursor)
                t.fail("physical vehicle step has no matching replay advance");
            t.waiting_for_vehicle=false;++t.mechanics;
        }else if(entry==0x41e2d0){
            if(t.mechanics!=t.active_count||slot!=6)t.fail("ranking ran before vehicle completion");
            ++t.rankings;
        }else t.fail("unexpected vehicle dispatch in normal physical race");
    }
    static void call(void* pointer,unsigned address)noexcept{
        auto& t=*static_cast<Trace*>(pointer);t.last_entry=address;
        if(address==0x418878){
            const auto slot=t.state.current_slot;
            if(slot>=6){t.fail("AI caller has invalid current vehicle");return;}
            const auto& race=t.state.vehicles.race[slot];const auto& physical=t.state.vehicles.physics[slot];
            // Actual408000 guards at this precise caller.407F60 changes only
            // prestart throttle state before the ordinary-rival branch.
            t.rival_pending=t.state.clock.game_mode==2&&t.state.clock.phase==2&&race.active!=0&&
                physical.special_mode!=1&&race.stopped!=1&&physical.energy_exempt!=1&&
                race.automatic_drive!=1&&physical.upgrade_active!=0;
            t.rival_slot=slot;
        }else if(address==0x418887&&t.rival_pending){
            if(t.state.current_slot!=t.rival_slot)t.fail("AI changed the source current vehicle");
            ++t.rival_dispatch_returns[t.rival_slot];t.rival_pending=false;
        }
    }
};

bool finite_car(const VehicleState& s){
    const std::array values{s.position.x,s.position.y,s.position.z,s.previous_position.x,
        s.previous_position.y,s.previous_position.z,s.velocity.x,s.velocity.y,s.velocity.z,
        s.speed,s.energy,s.effective_mass,s.orientation[0],s.orientation[1],s.orientation[2],
        s.orientation[4],s.orientation[5],s.orientation[6],s.orientation[8],s.orientation[9],s.orientation[10]};
    return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
}
unsigned player_slot(const RaceSessionState& s){
    for(const auto& controller:s.controls.controllers)if(controller.active==1&&controller.vehicle<6)
        return controller.vehicle;
    throw FormatError("normal contact session has no active player controller");
}
}

int main(int argc,char** argv)try{
    if(argc!=6)throw FormatError("expected retail-root TRACKn ticks neutral|drive output.csv");
    const auto requested=std::stoul(argv[3]);
    if(requested<1||requested>20000)throw FormatError("contact-session ticks outside 1..20000");
    const std::string profile=argv[4];
    if(profile!="neutral"&&profile!="drive")throw FormatError("unknown contact-session input profile");
    std::ofstream csv(argv[5]);if(!csv)throw FormatError("cannot create contact-session trace");
    csv<<std::setprecision(std::numeric_limits<float>::max_digits10)
       <<"tick,slot,mode,phase,active,automatic,controls,upgrade_active,energy_exempt,race_index,lap_index,progress,rank,speed,x,y,z,vx,vy,vz,energy,collision_hold,pair_events,throttle,left,right,throttle_signal,steering,start_countdown,lap_timer,race_timer,respawn,finish\n";
    RaceSessionLaunch launch;launch.level_name=argv[2];launch.music_percent=0;launch.master_gain=0;
    launch.mode=SessionMode::normal_race;
    RaceSession session(argv[1],launch);auto& s=session.state();
    Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
    PlayableDriver driver(session,textures,{});PlayableVehicleStages models(session);Trace trace(s);
    VehicleTickAudit calls{};calls.call_context=&trace;calls.before_call=Trace::call;
    VehicleWorldTickAudit vehicle{};vehicle.context=&trace;vehicle.before_stage=Trace::vehicle;vehicle.vehicle=&calls;
    SessionWorldTickAudit audit{};audit.context=&trace;audit.vehicle=&vehicle;
    audit.before_stage=Trace::before_world;audit.after_stage=Trace::after_world;
    std::array<std::array<std::uint32_t,6>,6> previous_counts{};
    std::array<std::uint64_t,6> pair_events{},automatic_steps{},rival_steps{},rival_throttle_steps{};
    std::array<double,6> traveled{},rival_traveled{};
    std::array<VehicleVector,6> positions{};
    for(unsigned i=0;i<6;++i){previous_counts[i]=s.vehicles.contact[i].pair_counters.count;positions[i]=s.vehicles.physics[i].position;}
    const auto player=player_slot(s);
    unsigned manual_tick{},manual_steps{},normal_steps{},all_six_steps{};
    try{for(unsigned n=0;n<requested;++n){
        trace.reset();PlayableInput input;
        const auto& race=s.vehicles.race[player];
        const bool manual=s.clock.game_mode==2&&s.clock.phase==2&&race.control_enabled==1&&race.automatic_drive==0;
        if(manual){
            if(manual_tick==0)manual_tick=s.simulation_tick+1;
            if(profile=="drive"){
                input.throttle=manual_steps<720;input.brake=manual_steps>=720&&manual_steps<840;
                input.left=manual_steps>=240&&manual_steps<270;
                input.right=manual_steps>=300&&manual_steps<330;
                input.turbo=manual_steps>=180&&manual_steps<240;
            }
            ++manual_steps;
        }
        auto camera=camera_frame_world(session);camera_frame_begin_outer(camera);
        driver.tick(input,&audit);
        maintain_session_audio(session);
        if(trace.error)throw FormatError(trace.error);
        if(trace.before!=7||trace.after!=7)throw FormatError("incomplete world stages");
        camera_frame_normal_views(camera,{},models,nullptr);
        if(s.clock.game_mode==2)++normal_steps;
        if(trace.active_count==6)++all_six_steps;
        for(unsigned i=0;i<6;++i){
            const auto& p=s.vehicles.physics[i];const auto& r=s.vehicles.race[i];
            if(!finite_car(p))throw FormatError("nonfinite physical state in contact session");
            if(r.active==1&&r.automatic_drive==1)++automatic_steps[i];
            const auto dx=double(p.position.x)-positions[i].x,dy=double(p.position.y)-positions[i].y,dz=double(p.position.z)-positions[i].z;
            const double distance=std::sqrt(dx*dx+dy*dy+dz*dz);
            traveled[i]+=distance;positions[i]=p.position;
            // Ordinary rivals use the recovered upgrade-active branch after
            // automatic rolling-start control has ended. Startup motion alone
            // is insufficient evidence of a racing opponent.
            if(i!=player&&r.active==1&&s.clock.game_mode==2&&s.clock.phase==2&&
               r.automatic_drive==0&&p.upgrade_active==1&&p.energy_exempt==0){
                ++rival_steps[i];rival_traveled[i]+=distance;
                if(p.throttle>0&&std::abs(p.speed)>5)++rival_throttle_steps[i];
            }
            const auto& counts=s.vehicles.contact[i].pair_counters.count;
            for(unsigned j=0;j<6;++j){
                const auto increment=counts[j]-previous_counts[i][j];
                if(increment>1)throw FormatError("pair event counter discontinuity");
                pair_events[i]+=increment;previous_counts[i][j]=counts[j];
            }
            csv<<s.simulation_tick<<','<<i<<','<<s.clock.game_mode<<','<<s.clock.phase<<','<<r.active<<','
               <<r.automatic_drive<<','<<r.control_enabled<<','<<p.upgrade_active<<','<<p.energy_exempt<<','
               <<r.race_index<<','<<r.lap_index<<','<<r.progress<<','<<r.place<<','
               <<p.speed<<','<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','<<p.velocity.x<<','
               <<p.velocity.y<<','<<p.velocity.z<<','<<p.energy<<','<<r.collision_hold<<','<<pair_events[i]<<','
               <<unsigned(p.input.throttle)<<','<<unsigned(p.input.left)<<','<<unsigned(p.input.right)<<','
               <<p.throttle<<','<<p.steering<<','<<r.start_countdown<<','<<r.lap_timer<<','<<r.race_timer<<','
               <<r.respawn_requested<<','<<r.finish_state<<'\n';
        }
    }}catch(const std::exception& error){
        std::cerr<<"tick="<<s.simulation_tick<<" entry=0x"<<std::hex<<trace.last_entry<<std::dec
                 <<" slot="<<trace.last_slot<<" error="<<error.what()<<'\n';return 1;
    }
    csv.close();if(!csv)throw FormatError("contact-session trace write failed");
    bool ai_moved=true;for(unsigned i=0;i<6;++i)
        if(i!=player&&!(trace.rival_dispatch_returns[i]>=60&&rival_steps[i]>=60&&
            rival_throttle_steps[i]>=60&&rival_traveled[i]>300))ai_moved=false;
    std::uint64_t total_pairs{};for(auto value:pair_events)total_pairs+=value;
    const bool passed=manual_tick!=0&&normal_steps>0&&all_six_steps>0&&ai_moved&&total_pairs>0;
    std::cout<<std::boolalpha<<std::setprecision(10)<<"{\"passed\":"<<passed<<",\"requested_ticks\":"<<requested
             <<",\"ticks\":"<<s.simulation_tick<<",\"normal_steps\":"<<normal_steps
             <<",\"manual_control_tick\":"<<manual_tick<<",\"manual_steps\":"<<manual_steps
             <<",\"all_six_steps\":"<<all_six_steps<<",\"contact_calls\":"<<trace.total_contacts
             <<",\"vehicle_calls\":"<<trace.total_mechanics<<",\"player_slot\":"<<player
             <<",\"five_ai_moved\":"<<ai_moved<<",\"directed_pair_events\":"<<total_pairs
             <<",\"overlay_uploads\":"<<images.uploads<<",\"slots\":[";
    for(unsigned i=0;i<6;++i){if(i)std::cout<<',';std::cout<<"{\"slot\":"<<i<<",\"automatic_steps\":"<<automatic_steps[i]
        <<",\"ordinary_rival_steps\":"<<rival_steps[i]<<",\"ordinary_rival_distance\":"<<rival_traveled[i]
        <<",\"ordinary_rival_throttle_steps\":"<<rival_throttle_steps[i]
        <<",\"ordinary_rival_dispatch_returns\":"<<trace.rival_dispatch_returns[i]
        <<",\"traveled_source_units\":"<<traveled[i]<<",\"directed_pair_events\":"<<pair_events[i]
        <<",\"final_race_index\":"<<s.vehicles.race[i].race_index
        <<",\"final_finish_state\":"<<s.vehicles.race[i].finish_state<<'}';}
    std::cout<<"],\"source_parity\":false,\"gpu_test\":false,\"authored_session\":true,\"poses_injected\":false}\n";
    return passed?0:2;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
