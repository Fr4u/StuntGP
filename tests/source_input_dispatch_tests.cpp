// Receiving cases from the sealed NEW source-input proposal.
// Linked against the production input, menu, pause and Session libraries.
// The only scripted implementation is GetKeyboardState at the OS boundary.
#include "sgn/source_input_dispatch.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_pause.hpp"
#include <algorithm>
#include <bit>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
using namespace sgn::retail;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
unsigned cases{};
template<class F> void check(const char* name,F action){action();++cases;std::cout<<"PASS "<<name<<'\n';}
auto bytes(const VehicleInput& i){return std::array<std::uint8_t,10>{i.throttle,i.brake,i.left,i.right,i.shift,i.stunt,i.rear_lock,i.steering_boost,i.turbo,i.auxiliary};}
auto values(const std::array<std::reference_wrapper<std::uint8_t>,7>& bank){
    std::array<std::uint8_t,7> out{};for(unsigned i=0;i<7;++i)out[i]=bank[i].get();return out;
}
struct Keys final:MenuKeyboardAcquisition {
    std::array<std::uint8_t,256> next{};unsigned calls{};std::function<void()> before;
    void acquire_45ec00(std::span<std::uint8_t,256> out)override{
        ++calls;if(before)before();std::copy(next.begin(),next.end(),out.begin());
    }
};
struct Fixture {
    RaceSession& session;RaceSessionState& s;SourceInputStorage& input;SessionPauseInvocation pause;
    explicit Fixture(RaceSession& owner):session(owner),s(owner.state()),input(s.source_input),pause(s,owner.audio_device()){
        // Reset test payloads in the SAME owner; never an application reset.
        SourceInputStorage initial;
        input.acquisition.upstream=initial.acquisition.upstream;
        input.acquisition.physical=initial.acquisition.physical;
        input.acquisition.source_device_pointer_576b6c=initial.acquisition.source_device_pointer_576b6c;
        input.physical=initial.physical;input.profiles=initial.profiles;
        input.vehicle_profiles=initial.vehicle_profiles;input.previous_activity=initial.previous_activity;
        // Explicit test seeds, never a proposed replacement for source startup.
        s.scene_mode=0;s.frame_pause=0;s.startup.field_60f2c0=0;s.current_slot=5;
        s.menu_input.previous_pause=0;s.menu_input.raw_words={};s.menu_input.edges={};
        s.startup.fields_62d480.fill(0);s.startup.fields_62dcf0.fill(0);
        s.normal_finish.confirm_pressed=0;s.normal_finish.confirm_consumed=0;s.pause.raw_activity=0;
        s.pause.physical_input.token=0;
        for(auto& b:s.frontend_launch.input_bindings)b={0xffffu,0};
        for(unsigned i=0;i<6;++i){s.vehicles.physics[i].input={};s.vehicles.restart[i].previous_input={};
            s.vehicles.physics[i].previous_shift=0;s.vehicles.physics[i].previous_stunt=0;
            s.vehicles.startup[i].field_2280=1;}
    }
    auto bank(unsigned i){return borrow_menu_input_bank(s.menu_input.edges[i],input.acquisition.physical[i].extra_edges);}
    std::span<std::uint8_t,13> profile(unsigned bank_index,unsigned index=0){
        return std::span<std::uint8_t,13>{reinterpret_cast<unsigned char*>(input.profiles[bank_index].data())+13*index,13};
    }
    void activate(unsigned physical,unsigned vehicle,unsigned kind){
        s.menu_input.raw_words[physical]={1,1};s.frontend_launch.input_bindings[physical]={vehicle,99};
        input.acquisition.physical[physical].word_0c=kind;
        input.acquisition.physical[physical].words_from_2c[2]=5000;
    }
    void button(unsigned physical,unsigned index,std::uint8_t value,bool previous=false){
        auto& raw=input.acquisition.physical[physical];const auto offset=(previous?0x88u:0x68u)+index;
        if(offset<0x168u)reinterpret_cast<unsigned char*>(raw.words_from_2c.data())[offset-0x2c]=value;
        else input.physical[physical].bytes_168_187[offset-0x168]=value;
    }
    SourcePhysicalInputView physical(unsigned i){
        auto& r=input.acquisition.physical[i];auto& x=input.physical[i];
        return {s.menu_input.raw_words[i].word_0,r.word_8,r.word_0c,x.activity_kind,x.profile_index,
            s.frontend_launch.input_bindings[i],x.feedback_mode,r.word_24,r.word_28,r,x.bytes_168_187,bank(i)};
    }
    SourceVehicleInputView vehicle(unsigned i){
        auto& p=s.vehicles.physics[i];auto& old=s.vehicles.restart[i].previous_input;
        return {s.vehicles.startup[i].field_2280,p.input,{old.throttle,old.brake,old.left,old.right,
            p.previous_shift,p.previous_stunt,old.rear_lock,old.steering_boost,old.turbo,old.auxiliary},
            input.vehicle_profiles[i],s.vehicles.race[i].control_kind,s.vehicles.parameters[i].steering_sensitivity};
    }
    template<class F> void with_world(F action){
        auto p=[&]<std::size_t... I>(std::index_sequence<I...>){return std::array<SourcePhysicalInputView,4>{physical(static_cast<unsigned>(I))...};}(std::make_index_sequence<4>{});
        auto v=[&]<std::size_t... I>(std::index_sequence<I...>){return std::array<SourceVehicleInputView,6>{vehicle(static_cast<unsigned>(I))...};}(std::make_index_sequence<6>{});
        auto m=menu_physical_producer_views(input.acquisition,s.menu_input);
        const auto keys=menu_keyboard_bytes(s.startup);
        const std::span<const std::uint8_t,256> old{reinterpret_cast<const unsigned char*>(s.startup.fields_62dcf0.data()),256};
        SourceInputWorld w{s.frame_pause,keys,old,p,v,input.profiles,s.current_slot,input.acquisition.source_device_pointer_576b6c,
            s.normal_finish.confirm_pressed,s.normal_finish.confirm_consumed,s.pause.raw_activity,input.previous_activity,
            {s.scene_mode,pause.lifecycle()},{s.scene_mode,keys,m,source_menu_selectors(input.profiles),input.acquisition.source_device_pointer_576b6c}};
        action(w);
    }
};
}

int main(int argc,char** argv)try{
    require(argc==2,"supply the new project's retail data root; no old project path");
    check("profile-overlap-is-live-not-selector-copy",[]{
        SourceInputStorage in;auto selector=source_menu_selectors(in.profiles);
        require(&selector.word_473984==&in.profiles[2][3],"DWORD alias owner");
        auto* raw=reinterpret_cast<unsigned char*>(in.profiles[2].data());
        require(&selector.byte_473982==raw+10 && &selector.byte_473983==raw+11,"BYTE alias owner");
        in.profiles[2][3]=0x78563412u;raw[10]=0xa5;raw[11]=0x5a;
        require(selector.word_473984==0x78563412u&&selector.byte_473982==0xa5&&selector.byte_473983==0x5a,"selectors stale");
        require(source_input_profile(in.profiles,2,0)[12]==0x12&&source_input_profile(in.profiles,2,1)[0]==0x34,"13-byte boundary alias lost");
    });
    check("263-scale-two-rounded-operations-ftol64-low32",[]{
        struct Row{std::int32_t axis,range;std::uint32_t expected;};
        constexpr std::array<Row,19> rows{{{0,0,0},{1,0,0},{(-2147483647-1),0,0},
            {5000,5000,0x107u},{-5000,5000,0xfffffef9u},{2500,5000,0x83u},{-2500,5000,0xffffff7du},
            {475,5000,0x18u},{4850,5000,0xffu},{2147483647,1,0x7ffffef9u},{(-2147483647-1),1,0x80000000u},
            {2147483647,-1,0x80000107u},{(-2147483647-1),-1,0x80000000u},{2147483647,2147483647,0x107u},
            {(-2147483647-1),(-2147483647-1),0x107u},{1,(-2147483647-1),0},{-1,2147483647,0},
            {2147483647,5000,0x06bb98c7u},{(-2147483647-1),5000,0xf9446739u}}};
        for(const auto& r:rows)require(source_analog_axis_40c780(std::bit_cast<std::uint32_t>(r.axis),std::bit_cast<std::uint32_t>(r.range))==r.expected,"analog low32 mismatch");
    });
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;
    RaceSession session(argv[1],launch);
    check("pause-exact-one-calls-real-menu-before-all-vehicle-writes",[&]{
        Fixture f(session);f.activate(0,0,1);f.s.scene_mode=1;f.s.frame_pause=1;
        f.s.vehicles.physics[0].input={11,12,13,14,15,16,17,18,19,20};
        const auto before=bytes(f.s.vehicles.physics[0].input);menu_keyboard_bytes(f.s.startup)[0x26]=0x81;
        auto b=f.bank(0);for(unsigned i=0;i<7;++i)b.current[i].get()=static_cast<std::uint8_t>(71+i);
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"paused result");});
        require(bytes(f.s.vehicles.physics[0].input)==before&&f.s.current_slot==5,"paused vehicle write");
        require(values(b.previous)==std::array<std::uint8_t,7>{71,72,73,74,75,76,77},"menu previous seven bytes");
        require(values(b.current)==std::array<std::uint8_t,7>{0,0,1,0,0,0,0},"actual menu publication");
    });
    check("raw-pause-two-is-normal-and-keyboard-priorities-copy-ten-history-bytes",[&]{
        Fixture f(session);f.activate(0,2,1);f.s.frame_pause=2;
        f.s.vehicles.physics[2].input={11,12,13,14,15,16,17,18,19,20};
        auto keys=menu_keyboard_bytes(f.s.startup);for(const auto k:{0x26u,0x28u,0x25u,0x27u,0x20u})keys[k]=0x80;
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"normal result");
            for(std::size_t i=0;i<10;++i)require(w.vehicles[2].previous[i].get()==static_cast<std::uint8_t>(11+i),"split previous owners");});
        require(bytes(f.s.vehicles.physics[2].input)==std::array<std::uint8_t,10>{255,0,0,255,255,0,0,0,0,0},"keyboard priorities/raw pause");
        require(f.s.current_slot==2&&f.input.acquisition.source_device_pointer_576b6c==0x62d5ac,"source identities");
    });
    check("physical-guards-and-disabled-vehicle-retain-exact-prefix",[&]{
        Fixture f(session);for(unsigned i=0;i<4;++i){f.activate(i,i,1);auto b=f.bank(i);b.current[0].get()=static_cast<std::uint8_t>(40+i);}
        f.s.menu_input.raw_words[0].word_0=2;f.input.acquisition.physical[1].word_8=1;
        f.s.frontend_launch.input_bindings[2].vehicle=0xffffu;f.s.vehicles.startup[3].field_2280=0;
        f.s.vehicles.physics[3].input.throttle=91;f.input.physical[3].feedback_mode=2;
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"disabled reached feedback");});
        for(unsigned i=0;i<3;++i)require(f.bank(i).current[0].get()==static_cast<std::uint8_t>(40+i),"guard changed physical edges");
        require(f.bank(3).current[0].get()==0&&f.bank(3).previous[0].get()==43,"disabled edge copy missing");
        require(f.s.current_slot==3&&f.s.vehicles.physics[3].input.throttle==91,"disabled vehicle prefix");
    });
    check("all-six-kinds-profile-index-three-and-source-sensitivity",[&]{
        constexpr std::array<unsigned,6> banks{0,1,3,4,2,4};constexpr std::array<std::int32_t,6> controls{2,0,3,4,1,4};
        for(unsigned kind=1;kind<=6;++kind){Fixture f(session);f.activate(0,1,kind);f.input.physical[0].profile_index=3;
            for(unsigned b=0;b<5;++b)for(unsigned i=0;i<4;++i){auto p=f.profile(b,i);std::fill(p.begin(),p.end(),static_cast<std::uint8_t>(20+16*b+i));}
            f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"kind result");});
            for(auto x:f.input.vehicle_profiles[1])require(x==static_cast<std::uint8_t>(20+16*banks[kind-1]+3),"wrong profile bank/stride");
            require(f.s.vehicles.race[1].control_kind==controls[kind-1],"wrong control kind");
            require(std::bit_cast<std::uint32_t>(f.s.vehicles.parameters[1].steering_sensitivity)==(kind==3?0x3f4ccccdu:0x3f800000u),"wrong sensitivity");}
    });
    check("analog-horizontal-signs-and-digital-independent-pedals",[&]{
        Fixture f(session);f.activate(0,0,2);auto& raw=f.input.acquisition.physical[0];raw.words_from_2c[3]=2500;raw.words_from_2c[4]=std::bit_cast<std::uint32_t>(-2500);
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"analog result");});
        require(f.s.vehicles.physics[0].input.throttle==131&&f.s.vehicles.physics[0].input.left==131&&f.s.vehicles.physics[0].input.right==0,"analog sign/263 scale");
        raw.word_0c=4;auto p=f.profile(4);p[0]=4;p[1]=5;p[2]=1;p[3]=1;raw.words_from_2c[3]=0xfffff63bu;
        f.button(0,4,1);f.button(0,5,1);
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"digital result");});
        require(f.s.vehicles.physics[0].input.throttle==255&&f.s.vehicles.physics[0].input.brake==255&&f.s.vehicles.physics[0].input.left==255,"digital priorities/threshold");
    });
    check("selector255-reads-old-tail-and-refused-pause-publishes-token",[&]{
        Fixture f(session);f.activate(0,0,2);f.s.scene_mode=1;f.s.startup.field_60f2c0=1;f.profile(1)[12]=255;
        f.button(0,255,1);f.button(0,255,1,true);
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"old held result");});
        require(f.s.pause.physical_input.token==0,"held old255 spuriously toggled");f.button(0,255,0,true);
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"edge result");});
        require(f.s.frame_pause==0&&f.s.pause.physical_input.token==1,"real40C750 refused-entry ordering");
    });
    check("activity-history-confirm-short-circuit-and-nonwrapping-abs",[&]{
        Fixture f(session);f.activate(0,0,1);f.activate(1,1,2);f.activate(2,2,5);
        f.s.normal_finish.confirm_pressed=37;f.s.pause.raw_activity=49;f.input.physical[2].activity_kind=91;
        menu_keyboard_bytes(f.s.startup)[0x26]=0x80;f.button(1,1,1);
        f.with_world([&](SourceInputWorld& w){update_source_input_activity_45f110(w);});
        require(f.s.normal_finish.confirm_consumed==37&&f.input.previous_activity==49,"activity history");
        require(f.s.pause.raw_activity==1&&f.s.normal_finish.confirm_pressed==1&&f.input.physical[1].activity_kind==2&&f.input.physical[2].activity_kind==91,"stopped on activity or ignored confirm");
        auto keyboard=menu_keyboard_bytes(f.s.startup);std::fill(keyboard.begin(),keyboard.end(),std::uint8_t{0});
        f.button(1,1,0);f.input.acquisition.physical[1].words_from_2c[3]=0x80000000u;
        f.with_world([&](SourceInputWorld& w){update_source_input_activity_45f110(w);});
        require(f.s.pause.raw_activity==1&&f.s.normal_finish.confirm_pressed==0&&f.input.physical[2].activity_kind==4,"FABS INT_MIN or confirm scan");
    });
    check("feedback-publication-and-missing-child-stop-before-next-record",[&]{
        Fixture f(session);f.activate(0,0,99);f.activate(1,1,1);f.input.acquisition.physical[0].word_24=2;
        f.bank(1).current[0].get()=91;f.s.vehicles.race[0].control_kind=73;
        f.with_world([&](SourceInputWorld& w){publish_input_feedback_kind_45ef00(w.physical);const auto r=dispatch_source_input_40dad0(w);
            require(r.status==SourceInputStatus::force_feedback_410a10_required&&r.physical_slot==0,"FF silently skipped");});
        require(f.input.physical[0].feedback_mode==2&&f.input.acquisition.physical[0].word_28==2,"feedback publish");
        require(f.bank(1).current[0].get()==91&&f.s.vehicles.race[0].control_kind==73&&f.input.acquisition.source_device_pointer_576b6c==0x62d5ac,"wrong missing-child prefix");
    });
    check("keyboard-zero-key-distinguishes-driving-from-activity-and-pause",[&]{
        Fixture f(session);f.activate(0,0,1);auto p=f.profile(0);std::fill(p.begin(),p.end(),std::uint8_t{0});
        menu_keyboard_bytes(f.s.startup)[0]=0x80;f.s.scene_mode=1;f.s.startup.field_60f2c0=1;
        f.with_world([&](SourceInputWorld& w){update_source_input_activity_45f110(w);
            require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"zero key result");});
        require(bytes(f.s.vehicles.physics[0].input)==std::array<std::uint8_t,10>{},"driving treated zero key as a binding");
        require(f.s.normal_finish.confirm_pressed==1&&f.s.pause.raw_activity==1&&f.s.pause.physical_input.token==1,"activity/pause incorrectly skipped zero key");
    });
    check("first-device-zero-range-and-profile-boundary-errors-are-explicit",[&]{
        Fixture f(session);f.activate(0,0,2);f.input.acquisition.physical[0].words_from_2c[2]=0;
        f.input.acquisition.physical[0].words_from_2c[3]=0x80000000u;
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w).status==SourceInputStatus::completed,"zero range blocked ordinary acquisition failure");});
        require(bytes(f.s.vehicles.physics[0].input)==std::array<std::uint8_t,10>{},"zero range did not keep FISTP indefinite lowDWORD0");
        f.input.physical[0].profile_index=4;f.s.vehicles.race[0].control_kind=91;
        bool stopped=false;f.with_world([&](SourceInputWorld& w){try{(void)dispatch_source_input_40dad0(w);}catch(const sgn::FormatError&){stopped=true;}});
        require(stopped&&f.s.vehicles.race[0].control_kind==0&&f.s.current_slot==0,"invalid profile silently accepted or prefix rolled back");
    });
    check("actual-session-prefix-history-pause-menu-and-resume-leaf",[&]{
        Fixture f(session);f.activate(0,0,1);f.s.scene_mode=1;Keys keys;MenuDirectInputRegistry registry;
        // Keyboard-only fixture seeding is deliberately not actual DI startup.
        initialize_keyboard_upstream_projection_4249b0(f.input.acquisition);
        const auto tick=f.s.simulation_tick;menu_keyboard_bytes(f.s.startup)[0x26]=0x83;keys.next[0x26]=0x80;
        keys.before=[&]{require(reinterpret_cast<const unsigned char*>(f.s.startup.fields_62dcf0.data())[0x26]==0x83,"history copied after OS acquisition");};
        require(acquire_dispatch_session_input(session,keys,registry).status==SourceInputStatus::completed,"session normal result");
        require(f.s.vehicles.physics[0].input.throttle==255&&f.s.vehicles.restart[0].previous_input.throttle==0,"session binder wrong current/history owners");
        require(f.s.normal_finish.confirm_pressed==0&&f.s.pause.raw_activity==1&&f.input.physical[0].activity_kind==5,"session45F210 suffix missing");
        keys.before={};keys.next.fill(0);keys.next[0x1b]=0x80;
        require(acquire_dispatch_session_input(session,keys,registry).status==SourceInputStatus::completed,"pause result");
        require(f.s.frame_pause==1&&f.s.pause.physical_input.token==1,"actual pause child not reached");
        keys.next.fill(0);keys.next[0x0d]=0x80;
        require(acquire_dispatch_session_input(session,keys,registry).status==SourceInputStatus::completed,"paused menu result");
        require(f.s.menu_input.edges[0].activate==1&&f.s.frame_pause==1,"paused route did not publish menu");
        // Invoke the real resume leaf; outer menu action dispatch is another stage.
        resume_session_pause(session);
        keys.next.fill(0);keys.next[0x26]=0x80;
        require(acquire_dispatch_session_input(session,keys,registry).status==SourceInputStatus::completed,"resumed input result");
        require(f.s.frame_pause==0&&f.s.vehicles.physics[0].input.throttle==255&&keys.calls==4,"input did not resume");
        require(f.s.simulation_tick==tick,"input producer advanced simulation itself");
    });
    std::cout<<"source_input_receiving_cases="<<cases<<" original_execution_cases=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
