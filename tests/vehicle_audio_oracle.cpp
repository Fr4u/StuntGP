#include "audio_oracle_state.hpp"
#include "sgn/vehicle_audio.hpp"
#include "sgn/retail_audio_catalog.hpp"
#include "sgn/vehicle_camera_settings.hpp"
static_assert(sgn::retail::audio_sample_definitions.size()==71);
namespace {
using FullRecord=std::array<std::uint32_t,1248>;
struct CarState {VehicleParameters p;VehicleState s;VehicleRaceState race;contact::solver::State contact;VehicleLoadState load;VehicleAudioState audio;};
void car_transfer(FullRecord& r,std::size_t base,CarState& c,bool loading) {
    auto u=[&](std::uint32_t& x,std::size_t i){if(loading)x=r[base+i];else r[base+i]=x;};
    auto si=[&](std::int32_t& x,std::size_t i){if(loading)x=std::bit_cast<std::int32_t>(r[base+i]);else r[base+i]=std::bit_cast<std::uint32_t>(x);};
    auto f=[&](float& x,std::size_t i){if(loading)x=real(r[base+i]);else r[base+i]=bits(x);};
    f(c.p.rpm_max,0);f(c.p.turbo_max,1);f(c.s.position.x,2);f(c.s.position.y,3);f(c.s.position.z,4);
    f(c.s.velocity.x,5);f(c.s.velocity.y,6);f(c.s.velocity.z,7);f(c.s.speed,8);f(c.s.steering,9);f(c.s.throttle,10);f(c.s.rpm,11);
    f(c.s.turbo_rpm,12);f(c.s.average_load,13);si(c.s.contact_class,14);si(c.s.gear,15);
    if(loading){if(r[base+16]>255)throw std::runtime_error("audio auxiliary byte outside domain");c.s.input.auxiliary=static_cast<std::uint8_t>(r[base+16]);}
    else r[base+16]=c.s.input.auxiliary;
    u(c.s.launch_event,17);f(c.s.launch_event_strength,18);si(c.s.gear_changed,19);
    for(unsigned i=0;i<4;++i){const auto at=20+i*6;auto& w=c.s.wheels[i];auto& h=c.contact.wheels[i];
        f(w.speed,at);f(w.load,at+1);f(w.reference_load,at+2);f(w.grip_usage,at+3);si(h.type,at+4);si(h.changed,at+5);}
    f(c.contact.shock,44);f(c.contact.pair_shock,45);si(c.contact.shock_hold,46);u(c.contact.effect_2368,47);si(c.contact.wheel_touched,48);
    auto& a=c.audio;u(a.horn_handle,49);for(unsigned i=0;i<4;++i)u(a.fields_233c[i],50+i);
    u(a.effect_hold,54);u(a.field_2350,55);u(a.effect_pending,56);u(a.field_2358,57);f(a.effect_strength,58);
    u(a.impact_handle,59);u(a.previous_wheel_touched,60);u(a.grip_handle,61);u(a.scrape_handle,62);u(a.rolling_handle,63);u(a.boost_handle,64);
    u(a.idle_handle,65);u(a.rev_handle,66);u(a.motion_handle,67);u(a.field_7718,68);u(a.field_771c,69);u(a.field_7720,70);
    si(c.load.sounds.horn,71);si(c.load.sounds.engine_rev,72);si(c.load.sounds.engine_idle,73);si(c.load.sounds.engine_boost,74);si(c.race.active,75);
}
}
int main(int argc,char** argv)try {
    if(argc!=2)throw std::runtime_error("expected vehicle audio corpus");
    std::ifstream f(argv[1],std::ios::binary);std::array<char,8> magic{};read(f,magic);
    if(std::string_view(magic.data(),8)!="SGNVAU01")throw std::runtime_error("bad vehicle audio corpus magic");
    std::uint32_t count{};read(f,count);if(count==0||count>20000)throw std::runtime_error("bad vehicle audio case count");
    std::size_t mismatches{},words{},events{},continuous{};
    AudioDriverState state;LevelAudioState level;contact::player::Controls controls;std::array<CarState,6> cars;
    VehicleAudioGlobals globals;VehicleAudioFrontier frontier;FullRecord previous{};std::uint32_t previous_op=99;
    for(std::uint32_t n=0;n<count;++n) {
        std::uint32_t op{};read(f,op);if(op>13)throw std::runtime_error("invalid vehicle audio operation");
        FullRecord r{},expected{};read(f,r);read(f,expected);std::uint32_t event_count{};read(f,event_count);
        if(event_count>512)throw std::runtime_error("vehicle audio event count limit");
        std::vector<Event> expected_trace(event_count);for(auto& e:expected_trace)read(f,e);
        Record base{};std::copy_n(r.begin(),640,base.begin());Device device(base);
        const bool continuing=op==13&&r[1232]!=0;
        if(op==13&&r[1232]>=16)throw std::runtime_error("invalid continuous audio step");
        if(continuing) {
            if(previous_op!=13||previous[1232]+1!=r[1232])throw std::runtime_error("broken continuous audio step order");
            previous[1232]=r[1232];if(previous!=r)throw std::runtime_error("broken continuous native audio state");++continuous;
        }else {
            transfer(base,state,level,controls,true);
            for(unsigned i=0;i<6;++i)car_transfer(r,640+i*96,cars[i],true);
        }
        AudioDriver driver{state,device,level,controls,base[8]};
        auto wrap=[&](unsigned i)->VehicleAudioCar {auto& c=cars[i];return {c.p,c.s,c.race,c.contact,c.load,c.audio,r[640+i*96+76]};};
        std::array<VehicleAudioCar,6> view{wrap(0),wrap(1),wrap(2),wrap(3),wrap(4),wrap(5)};
        const auto current=r[1216];if(current>=6||r[1217]>=4)throw std::runtime_error("vehicle audio selection outside domain");
        if(!continuing) {
            controls.current=r[1217];for(unsigned i=0;i<4;++i)controls.controllers[i].vehicle=r[1228+i];
            globals={r[1219],real(r[1220])};frontier={real(r[1221]),r[1222],r[1223],r[1224]};
        }
        VehicleAudioWorld world{view,driver,controls,globals,frontier,r[1218]};const VehicleAudioBindings b{view[current],driver,controls,globals,frontier};
        switch(op) {
        case 0:vehicle_audio_grip(b);break;case 1:vehicle_audio_rolling(b);break;case 2:vehicle_audio_landing(b);break;
        case 3:vehicle_audio_scrape(b);break;case 4:vehicle_audio_collision(b);break;case 5:vehicle_audio_boost(b);break;
        case 6:vehicle_audio_surface_event(b);break;case 7:vehicle_audio_events(b);break;case 8:vehicle_audio_motion(b);break;
        case 9:vehicle_audio_engine(b);break;case 10:vehicle_audio_steering(b);break;case 11:reset_vehicle_audio(world);break;
        case 12:case 13:update_vehicle_audio(world,current);break;default:throw std::runtime_error("unreachable vehicle audio operation");
        }
        transfer(base,state,level,controls,false);std::copy(base.begin(),base.end(),r.begin());
        for(unsigned i=0;i<6;++i)car_transfer(r,640+i*96,cars[i],false);
        r[1217]=controls.current;r[1219]=globals.controller_gain;r[1220]=bits(globals.motion_gain);
        r[1221]=bits(frontier.previous_steering);r[1222]=frontier.rotating_impact;r[1223]=frontier.steering_handle;r[1224]=frontier.steering_hold;
        previous=r;previous_op=op;
        for(std::size_t i=0;i<r.size();++i){++words;if(r[i]!=expected[i]){
            if(mismatches<15)std::cerr<<"case="<<n<<" op="<<op<<" word="<<i<<" actual="<<std::hex<<r[i]<<" expected="<<expected[i]<<std::dec<<'\n';++mismatches;}}
        if(device.trace!=expected_trace){if(mismatches<15){std::cerr<<"case="<<n<<" op="<<op<<" trace native="<<device.trace.size()<<" source="<<expected_trace.size()<<'\n';
            for(std::size_t j=0;j<std::min(device.trace.size(),expected_trace.size());++j)if(device.trace[j]!=expected_trace[j]) {
                for(auto x:device.trace[j])std::cerr<<std::hex<<x<<',';std::cerr<<" vs ";for(auto x:expected_trace[j])std::cerr<<x<<',';std::cerr<<std::dec<<'\n';break;}}
            ++mismatches;}
        events+=device.trace.size();
    }
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing vehicle audio corpus data");
    std::cout<<"cases="<<count<<" words="<<words<<" device_events="<<events<<" continuous_native_steps="<<continuous<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
    return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<"vehicle audio oracle: "<<e.what()<<'\n';return 2;}
