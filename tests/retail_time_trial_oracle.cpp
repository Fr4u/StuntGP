#include "replay_fixture_cursor_codec.hpp"
#include "sgn/retail_time_trial.hpp"
#include "contact_solver_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
using namespace sgn::retail;
using Record=std::vector<std::uint8_t>;
constexpr unsigned cs=0x7800,ca=0x2d000,p=0x2f000,sa=0x30000,bank=0x32000,size=0xcd000,g=p+0xb00;
std::uint32_t u(const Record& b,unsigned at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
void put(Record& b,unsigned at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
float f(const Record& b,unsigned at){return std::bit_cast<float>(u(b,at));}
std::int32_t i(const Record& b,unsigned at){return std::bit_cast<std::int32_t>(u(b,at));}
bool apply(Record& b,unsigned entry,SplineView spline) {
    if(u(b,p)>=6||u(b,p+4)>=6||u(b,p+0x34)>=4||u(b,g+0x10)>=4)throw std::runtime_error("Time Trial selector");
    auto parameters=std::make_unique<std::array<VehicleParameters,6>>();
    auto physics=std::make_unique<std::array<VehicleState,6>>();
    auto contact=std::make_unique<std::array<contact::solver::State,6>>();
    std::array<VehicleRaceState,6> races{};std::array<AiAutomaticMemory,6> ai{};
    auto transfer=[&](bool read){for(unsigned j=0;j<6;++j){
        auto bytes=std::span(b).subspan(j*cs,cs);
        contact::oracle::transfer(bytes,(*parameters)[j],(*physics)[j],races[j],(*contact)[j],read);
        if(read){const auto slot=u(b,j*cs+0x7268);if(slot!=0xffffffff){if(slot>=6)throw std::runtime_error("recording slot");races[j].recording=ReplayHandle{slot};}ai[j].steering_ramp_ticks=u(b,j*cs+0x26a4);}
        else{put(b,j*cs+0x7268,races[j].recording?races[j].recording->slot:0xffffffff);put(b,j*cs+0x26a4,ai[j].steering_ramp_ticks);}
    }};transfer(true);
    std::array<ResetVehicle,6> vehicles{{
        {(*physics)[0],races[0],(*contact)[0],0},{(*physics)[1],races[1],(*contact)[1],1},
        {(*physics)[2],races[2],(*contact)[2],2},{(*physics)[3],races[3],(*contact)[3],3},
        {(*physics)[4],races[4],(*contact)[4],4},{(*physics)[5],races[5],(*contact)[5],5}}};
    contact::player::Controls controls;controls.current=u(b,p+0x34);
    std::array<ControllerOverlayTransition,4> overlays{};std::array<ControllerHudCountdown,4> hud{};
    static_assert(sizeof(ControllerOverlayTransition)==40&&sizeof(VehicleScorePopup)==0x138);
    auto controllers=[&](bool read){for(unsigned j=0;j<4;++j){const auto at=ca+j*0x74c;auto& c=controls.controllers[j];
        if(read){c.active=i(b,at);c.vehicle=u(b,at+0xac);if(c.vehicle>=6)throw std::runtime_error("controller vehicle");
            c.camera.mode=u(b,at+4);std::memcpy(&c.camera.position,b.data()+at+8,16);
            c.camera.twist=f(b,at+0x78);c.camera.collision_bypass_ticks=u(b,at+0x1a4);
            c.respawn_transition.state=u(b,at+0x300);c.respawn_transition.phase=u(b,at+0x308);
            std::memcpy(c.notices.data(),b.data()+at+0x4ac,sizeof(c.notices));
            std::memcpy(&overlays[j],b.data()+at+0x1e8,40);
            hud[j]={u(b,at+0x210),u(b,at+0x214),u(b,at+0x228),f(b,at+0x22c),u(b,at+0x230)};
        }else{put(b,at,std::bit_cast<std::uint32_t>(c.active));put(b,at+0xac,c.vehicle);put(b,at+4,c.camera.mode);
            std::memcpy(b.data()+at+8,&c.camera.position,16);put(b,at+0x78,std::bit_cast<std::uint32_t>(c.camera.twist));
            put(b,at+0x1a4,c.camera.collision_bypass_ticks);put(b,at+0x300,c.respawn_transition.state);put(b,at+0x308,c.respawn_transition.phase);
            std::memcpy(b.data()+at+0x4ac,c.notices.data(),sizeof(c.notices));std::memcpy(b.data()+at+0x1e8,&overlays[j],40);
            put(b,at+0x210,hud[j].active);put(b,at+0x214,hud[j].ticks);put(b,at+0x228,hud[j].enabled);
            put(b,at+0x22c,std::bit_cast<std::uint32_t>(hud[j].value));put(b,at+0x230,hud[j].phase);
        }
    }};controllers(true);
    VehicleResetParameters config{};config.stage=i(b,p+8);config.mode=i(b,p+12);config.selector=i(b,p+16);
    config.tick=u(b,p+20);config.normal_energy_cost=f(b,p+24);config.upgraded_energy_cost=f(b,p+28);
    config.course_length=f(b,p+32);config.spacing_word=u(b,p+36);config.spline=spline;
    for(unsigned j=0;j<128;++j)config.mask[j]=f(b,p+0x100+j*4);
    config.unit_gravity={f(b,p+0x40),f(b,p+0x44),f(b,p+0x48),f(b,p+0x4c)};
    config.vehicle_names={"CAR 0","CAR 1","CAR 2","CAR 3","CAR 4","CAR 5"};config.special_end_text="Recovered";config.retired_text="Retired";
    VehicleResetGlobals globals{u(b,p+0x28),u(b,p+0x2c),u(b,p+0x30)};
    unsigned current=u(b,p+4),selected_hud=u(b,g+0x10);
    VehicleResetWorld reset{vehicles,current,controls,globals,config};
    VehicleScoreState score;std::memcpy(score.popups.data(),b.data()+sa,0x1380);
    score.sound_class=u(b,g+0x94);score.sound_id=u(b,g+0x98);score.sound_volume=u(b,g+0x9c);
    ReplayBank replays;std::memcpy(replays.bytes().data(),b.data()+bank,ReplayBank::size);
    ::sgn::retail::fixture::legacy_replay_cursor_words(replays,true);
    TimeTrialGhosts ghosts;ghosts.reference_completed=u(b,g+0x14);
    for(unsigned j=0;j<4;++j){const auto at=g+0x40+j*16;ghosts.roles[j]={u(b,at),u(b,at+4),ReplayHandle{u(b,at+8)},u(b,at+12)};}
    TimeTrialSelectionState selection;for(unsigned j=0;j<4;++j)selection.availability[j]=u(b,g+0x30+j*4);
    RaceClockState clock{u(b,p+12),u(b,p+8),u(b,g+0x18),u(b,g+0x1c),u(b,g+0x20),u(b,g+0x24),u(b,g+0x28),f(b,p+32)};
    LevelAudioState audio{u(b,g+0x80),u(b,g+0x84),u(b,g+0x88),u(b,g+0x8c),u(b,g+0x90),u(b,g+0xa0)};
    TimeTrialStartState state{u(b,g),u(b,g+4),u(b,g+8)};
    bool geometry=false;
    if(entry==0x443100){TimeTrialStartWorld world{vehicles,ai,replays,ghosts,selection,clock,controls,globals,config,score,overlays,hud,current,selected_hud,audio,state};geometry=start_time_trial(world,u(b,g+0xc));}
    else if(entry==0x412b10){geometry=place_vehicle_at_distance(reset,u(b,p),f(b,g+0xb0));current=reset.current;}
    else if(entry==0x442ad0)set_overlay_transition(overlays[controls.current],u(b,g+0xb4),u(b,g+0xb8),u(b,g+0xbc),u(b,g+0xc0),u(b,g+0xc4),u(b,g+0xc8),f(b,g+0xcc));
    else if(entry==0x447b70)set_hud_countdown(hud,controls.current,selected_hud,u(b,g+0xd0));
    else if(entry==0x460cb0)set_level_audio_output(audio,u(b,g+0xd4));
    else throw std::runtime_error("unknown Time Trial entry");
    transfer(false);controllers(false);put(b,p+4,current);put(b,p+0x34,controls.current);put(b,g+0x10,selected_hud);
    put(b,p+0x28,globals.transition_active);put(b,p+0x2c,globals.special_value);put(b,p+0x30,globals.special_timer);
    ::sgn::retail::fixture::legacy_replay_cursor_words(replays,false);
    std::memcpy(b.data()+sa,score.popups.data(),0x1380);std::memcpy(b.data()+bank,replays.bytes().data(),ReplayBank::size);
    put(b,g+0x94,score.sound_class);put(b,g+0x98,score.sound_id);put(b,g+0x9c,score.sound_volume);
    put(b,g+0x14,ghosts.reference_completed);for(unsigned j=0;j<4;++j){const auto at=g+0x40+j*16;const auto& x=ghosts.roles[j];put(b,at,x.active);put(b,at+4,x.vehicle);put(b,at+8,x.replay.slot);put(b,at+12,x.lap_ticks);}
    put(b,p+12,clock.game_mode);put(b,p+8,clock.phase);put(b,g+0x18,clock.countdown_pending);put(b,g+0x1c,clock.rolling_state);
    put(b,g+0x20,clock.race_mode);put(b,g+0x24,clock.remaining_ticks);put(b,g+0x28,clock.lap_count);put(b,p+32,std::bit_cast<std::uint32_t>(clock.course_length));
    put(b,g+0x80,audio.scale);put(b,g+0x84,audio.input_a);put(b,g+0x88,audio.input_b);put(b,g+0x8c,audio.output_a);put(b,g+0x90,audio.output_b);put(b,g+0xa0,audio.output_b_copy);
    put(b,g,state.reference_at_start);put(b,g+4,state.announcement_ticks);put(b,g+8,state.phase_ticks);
    return geometry;
}
void read(std::ifstream& in,void* out,std::size_t n){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(n)))throw std::runtime_error("short Time Trial corpus");}
unsigned word(std::ifstream& in){unsigned x;read(in,&x,4);return x;}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("expected Time Trial corpus and PMD");
    SplineSet set(sgn::read_file(argv[2]));if(set.descriptors().empty())throw std::runtime_error("missing spline");
    const auto spline=*set.find(set.descriptors().front().id);
    std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);if(std::string(magic,8)!="SGNTT001")throw std::runtime_error("Time Trial corpus schema");
    const auto count=word(in);if(count==0||count>20000)throw std::runtime_error("Time Trial corpus count");
    std::uint64_t exact=0,excluded=0;unsigned errors=0,geometry_count=0,policy=0;
    for(unsigned n=0;n<count;++n){const auto entry=word(in);Record actual(size),expected(size);read(in,actual.data(),size);read(in,expected.data(),size);
        const auto omissions=word(in);if(omissions>5)throw std::runtime_error("Time Trial opaque count");
        const auto target=entry==0x443100?0:u(actual,p);if(target>=6)throw std::runtime_error("Time Trial target slot");
        std::vector<unsigned> opaque;for(unsigned j=0;j<omissions;++j){const auto off=word(in);if(off>=size||off%4||std::find(opaque.begin(),opaque.end(),off)!=opaque.end())throw std::runtime_error("Time Trial invalid mask");opaque.push_back(off);}
        const auto old_right=u(actual,target*cs+0x1bd8),old_heading=u(actual,target*cs+0x24cc);
        const bool geometry=apply(actual,entry,spline);geometry_count+=geometry;
        std::vector<unsigned> required;
        if(entry==0x443100||entry==0x412b10){required.push_back(target*cs+0x24cc);if(geometry)for(const auto off:{0x1bd8u,0x1bf8u,0x72c4u,0x72e4u})required.push_back(target*cs+off);}
        std::sort(required.begin(),required.end());std::sort(opaque.begin(),opaque.end());if(required!=opaque)throw std::runtime_error("Time Trial mask differs from actual native path");
        if(!required.empty()){if(u(actual,target*cs+0x24cc)!=old_heading)throw std::runtime_error("heading W policy");++policy;}
        if(geometry){for(const auto off:{0x1bd8u,0x72c4u})if(u(actual,target*cs+off)!=old_right)throw std::runtime_error("right W policy");for(const auto off:{0x1bf8u,0x72e4u})if(u(actual,target*cs+off)!=old_heading)throw std::runtime_error("body W policy");policy+=4;}
        for(unsigned off=0;off<size;off+=4){if(std::binary_search(opaque.begin(),opaque.end(),off)){++excluded;continue;}if(u(actual,off)==u(expected,off)){++exact;continue;}if(errors++<30)std::cerr<<"entry "<<std::hex<<entry<<" case "<<std::dec<<n<<" offset "<<std::hex<<off<<" actual "<<u(actual,off)<<" expected "<<u(expected,off)<<'\n';}
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing Time Trial corpus");
    std::cout<<"{\"passed\":"<<(errors?"false":"true")<<",\"cases\":"<<count<<",\"compared_words\":"<<exact<<",\"errors\":"<<errors<<",\"excluded_opaque_words\":"<<excluded<<",\"geometry_cases\":"<<geometry_count<<",\"opaque_policy_words\":"<<policy<<",\"allowed_ulp\":0}\n";
    return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
